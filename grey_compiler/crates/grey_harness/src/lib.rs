use std::collections::{BTreeMap, HashMap};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::Instant;

use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};

use grey_backends::betti_rdl::{BettiConfig, BettiRdlBackend};
use grey_backends::{CodeGenerator, ProcessPlacement};
use grey_ir::IrBuilder;
use grey_lang::compile;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionResult {
    pub seed_used: u64,
    pub max_events: i32,
    pub runtime_processes: usize,
    pub spacing: i32,

    pub events_processed: u64,
    pub current_time: u64,
    pub execution_time_ns: u64,

    pub process_states: BTreeMap<usize, i32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComparisonResult {
    pub grey: ExecutionResult,
    pub cpp: ExecutionResult,

    pub events_match: bool,
    pub current_time_match: bool,
    pub state_differences: Vec<String>,

    pub parity_achieved: bool,
}

#[derive(Debug, Clone)]
pub struct HarnessConfig {
    pub seed: u64,
    pub max_events: i32,
    pub spacing: i32,

    pub demo_path: PathBuf,

    /// If set, uses this executable directly instead of building it via CMake.
    pub cpp_exe_override: Option<PathBuf>,
}

impl Default for HarnessConfig {
    fn default() -> Self {
        let workspace_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../..")
            .canonicalize()
            .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));

        Self {
            seed: 42,
            max_events: 1000,
            spacing: 1,
            demo_path: workspace_root.join("examples/sir_demo.grey"),
            cpp_exe_override: None,
        }
    }
}

pub fn run_harness(config: &HarnessConfig) -> Result<ComparisonResult> {
    let grey = execute_grey(&config.demo_path, config)?;
    let cpp = execute_cpp(&grey, config)?;

    let mut state_differences = Vec::new();
    let pids: std::collections::BTreeSet<usize> = grey
        .process_states
        .keys()
        .chain(cpp.process_states.keys())
        .copied()
        .collect();

    for pid in pids {
        let g = grey.process_states.get(&pid);
        let c = cpp.process_states.get(&pid);
        if g != c {
            state_differences.push(format!("pid {}: grey={:?} cpp={:?}", pid, g, c));
        }
    }

    let events_match = grey.events_processed == cpp.events_processed;
    let current_time_match = grey.current_time == cpp.current_time;
    let parity_achieved = events_match && current_time_match && state_differences.is_empty();

    Ok(ComparisonResult {
        grey,
        cpp,
        events_match,
        current_time_match,
        state_differences,
        parity_achieved,
    })
}

fn execute_grey(demo_path: &Path, config: &HarnessConfig) -> Result<ExecutionResult> {
    let source = std::fs::read_to_string(demo_path)
        .with_context(|| format!("reading Grey demo at {}", demo_path.display()))?;

    let start = Instant::now();

    let typed_program = compile(&source).map_err(|e| anyhow!("Grey compilation failed: {e}"))?;

    let mut builder = IrBuilder::new();
    let ir_program = builder
        .build_program("sir_demo", &typed_program)
        .context("IR build failed")?;

    let backend = BettiRdlBackend::new(BettiConfig {
        max_events: config.max_events,
        seed: config.seed,
        process_placement: ProcessPlacement::GridLayout {
            spacing: config.spacing,
        },
        telemetry_enabled: true,
        validate_coordinates: true,
    });

    let output = backend
        .generate_code(ir_program)
        .context("Betti codegen failed")?;

    let telemetry = backend.execute(&output).context("Betti execution failed")?;

    let mut process_states = BTreeMap::new();
    for (pid, state) in telemetry.process_states {
        process_states.insert(pid, state);
    }

    Ok(ExecutionResult {
        seed_used: config.seed,
        max_events: config.max_events,
        runtime_processes: output.metadata.runtime_process_count,
        spacing: config.spacing,
        events_processed: telemetry.events_processed,
        current_time: telemetry.current_time,
        execution_time_ns: start.elapsed().as_nanos() as u64,
        process_states,
    })
}

#[derive(Debug, Deserialize)]
struct CppJsonOutput {
    seed_used: u64,
    max_events: i32,
    runtime_processes: usize,
    spacing: i32,

    events_processed: u64,
    current_time: u64,

    process_states: HashMap<String, i32>,
}

fn execute_cpp(grey: &ExecutionResult, config: &HarnessConfig) -> Result<ExecutionResult> {
    let exe = match &config.cpp_exe_override {
        Some(path) => path.clone(),
        None => build_cpp_reference()?,
    };

    let output = Command::new(&exe)
        .arg("--seed")
        .arg(config.seed.to_string())
        .arg("--max-events")
        .arg(config.max_events.to_string())
        .arg("--processes")
        .arg(grey.runtime_processes.to_string())
        .arg("--spacing")
        .arg(config.spacing.to_string())
        .stdout(Stdio::piped())
        .stderr(Stdio::inherit())
        .output()
        .with_context(|| format!("running C++ reference exe at {}", exe.display()))?;

    if !output.status.success() {
        return Err(anyhow!(
            "C++ reference exe failed with status {:?}",
            output.status.code()
        ));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let json_line = stdout
        .lines()
        .rev()
        .find(|line| line.trim_start().starts_with('{'))
        .ok_or_else(|| anyhow!("C++ output did not contain a JSON line"))?;

    let parsed: CppJsonOutput = serde_json::from_str(json_line)
        .with_context(|| format!("parsing JSON from C++ output: {json_line}"))?;

    let mut process_states = BTreeMap::new();
    for (k, v) in parsed.process_states {
        let pid: usize = k
            .parse()
            .with_context(|| format!("invalid pid key in C++ output: {k}"))?;
        process_states.insert(pid, v);
    }

    Ok(ExecutionResult {
        seed_used: parsed.seed_used,
        max_events: parsed.max_events,
        runtime_processes: parsed.runtime_processes,
        spacing: parsed.spacing,
        events_processed: parsed.events_processed,
        current_time: parsed.current_time,
        execution_time_ns: 0,
        process_states,
    })
}

fn build_cpp_reference() -> Result<PathBuf> {
    let workspace_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../..")
        .canonicalize()
        .context("locating grey_compiler workspace root")?;

    let project_root = workspace_root
        .join("..")
        .canonicalize()
        .context("locating repo root")?;

    let cpp_kernel_dir = project_root.join("src/cpp_kernel");
    let build_dir = workspace_root.join("target/cpp_kernel_harness_build");

    std::fs::create_dir_all(&build_dir)
        .with_context(|| format!("creating build dir {}", build_dir.display()))?;

    let cmake_configure = Command::new("cmake")
        .arg("-S")
        .arg(&cpp_kernel_dir)
        .arg("-B")
        .arg(&build_dir)
        .arg("-DCMAKE_BUILD_TYPE=Release")
        .stdout(Stdio::inherit())
        .stderr(Stdio::inherit())
        .status()
        .context("cmake configure")?;

    if !cmake_configure.success() {
        return Err(anyhow!("cmake configure failed"));
    }

    let cmake_build = Command::new("cmake")
        .arg("--build")
        .arg(&build_dir)
        .arg("--target")
        .arg("grey_sir_reference")
        .stdout(Stdio::inherit())
        .stderr(Stdio::inherit())
        .status()
        .context("cmake build")?;

    if !cmake_build.success() {
        return Err(anyhow!("cmake build failed"));
    }

    let exe_name = if cfg!(windows) {
        "grey_sir_reference.exe"
    } else {
        "grey_sir_reference"
    };

    let candidates = [
        build_dir.join(exe_name),
        build_dir.join("Release").join(exe_name),
    ];

    candidates
        .into_iter()
        .find(|p| p.exists())
        .ok_or_else(|| anyhow!("built executable not found in {}", build_dir.display()))
}

pub fn print_summary(result: &ComparisonResult) {
    println!("Grey events_processed={} current_time={} runtime_processes={}", result.grey.events_processed, result.grey.current_time, result.grey.runtime_processes);
    println!("C++  events_processed={} current_time={} runtime_processes={}", result.cpp.events_processed, result.cpp.current_time, result.cpp.runtime_processes);

    if result.parity_achieved {
        println!("PARITY: OK");
    } else {
        println!("PARITY: FAILED");
        for diff in &result.state_differences {
            println!("  {diff}");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[ignore]
    fn sir_harness_end_to_end() {
        let config = HarnessConfig::default();
        let result = run_harness(&config).expect("harness run");
        assert!(result.parity_achieved, "parity must be achieved: {result:?}");
    }
}
