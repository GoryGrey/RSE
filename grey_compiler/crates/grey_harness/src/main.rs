use std::path::PathBuf;

use clap::Parser;

use grey_harness::{print_summary, run_harness, HarnessConfig};

#[derive(Parser, Debug)]
#[command(name = "grey_compare_sir")]
#[command(about = "Compile and run the Grey SIR demo and compare against the C++ reference")]
struct Cli {
    /// Path to the Grey demo program
    #[arg(long)]
    demo: Option<PathBuf>,

    /// Seed used for deterministic injection patterns
    #[arg(long, default_value = "42")]
    seed: u64,

    /// Maximum events to process
    #[arg(long, default_value = "1000")]
    max_events: i32,

    /// Grid spacing used when spawning processes
    #[arg(long, default_value = "1")]
    spacing: i32,

    /// Use an already-built C++ reference executable
    #[arg(long)]
    cpp_exe: Option<PathBuf>,
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    let mut config = HarnessConfig::default();
    config.seed = cli.seed;
    config.max_events = cli.max_events;
    config.spacing = cli.spacing;

    if let Some(demo) = cli.demo {
        config.demo_path = demo;
    }

    config.cpp_exe_override = cli.cpp_exe;

    let result = run_harness(&config)?;
    print_summary(&result);

    if !result.parity_achieved {
        std::process::exit(1);
    }

    Ok(())
}
