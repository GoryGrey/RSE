//! Grey Compiler CLI - Minimal Version
//! 
//! Command-line interface for the Grey programming language compiler.

use clap::{Parser, Subcommand};
use grey_lang::compile;
use grey_ir::{IrBuilder, IrProgram};
use grey_backends::betti_rdl::{BettiRdlBackend, BettiConfig};
use grey_backends::CodeGenerator;
use std::fs;
use std::io::{self, Write};
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "greyc")]
#[command(about = "Grey Programming Language Compiler")]
#[command(version = "0.1.0")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Check a Grey source file for errors
    Check {
        /// Input file to check
        input: PathBuf,
    },
    
    /// Start an interactive REPL
    Repl,
    
    /// Emit Betti RDL executable from Grey source
    EmitBetti {
        /// Input Grey source file
        input: PathBuf,
        
        /// Run the generated executable
        #[arg(long)]
        run: bool,
        
        /// Maximum events to process
        #[arg(long, default_value = "1000")]
        max_events: i32,
        
        /// Enable telemetry output
        #[arg(long)]
        telemetry: bool,
    },
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    
    match cli.command {
        Commands::Check { input } => {
            if !input.exists() {
                anyhow::bail!("Input file '{}' does not exist", input.display());
            }
            
            if !input.extension().map_or(false, |ext| ext == "grey") {
                anyhow::bail!("Input file must have .grey extension");
            }
            
            let source = fs::read_to_string(&input)?;
            println!("Checking '{}'...", input.display());
            
            match compile(&source) {
                Ok(_) => {
                    println!("✅ No errors found. Program is valid Grey.");
                    Ok(())
                }
                Err(e) => {
                    println!("❌ Compilation failed:");
                    println!("{:?}", e);
                    std::process::exit(1);
                }
            }
        }
        
        Commands::EmitBetti { input, run, max_events, telemetry } => {
            if !input.exists() {
                anyhow::bail!("Input file '{}' does not exist", input.display());
            }
            
            if !input.extension().map_or(false, |ext| ext == "grey") {
                anyhow::bail!("Input file must have .grey extension");
            }
            
            let source = fs::read_to_string(&input)?;
            println!("Compiling '{}' to Betti RDL...", input.display());
            
            // Compile Grey source
            let typed_program = compile(&source)
                .map_err(|e| anyhow::anyhow!("Compilation failed: {:?}", e))?;
            
            println!("✅ Compilation successful");
            
            // Build IR
            let program_name = input.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("program");
            
            let mut ir_builder = IrBuilder::new();
            let ir_program = ir_builder.build_program(program_name, &typed_program)
                .map_err(|e| anyhow::anyhow!("IR building failed: {}", e))?;
            
            println!("✅ IR built successfully: {} processes, {} events", 
                     ir_program.processes.len(), ir_program.events.len());
            
            // Generate Betti RDL code
            let backend = BettiRdlBackend::new(grey_backends::betti_rdl::BettiConfig {
                max_events,
                process_placement: grey_backends::ProcessPlacement::GridLayout { spacing: 4 },
                telemetry_enabled: telemetry,
                validate_coordinates: true,
            });
            
            let output = backend.generate_code(ir_program)
                .map_err(|e| anyhow::anyhow!("Code generation failed: {}", e))?;
            
            println!("✅ Betti RDL code generated");
            
            // Write generated files
            let output_dir = input.parent().unwrap_or_else(|| PathBuf::from("."));
            let betti_file = output_dir.join(format!("{}_betti.rs", program_name));
            
            if let Some((path, content)) = output.files.iter().find(|(path, _)| {
                path.to_string_lossy().contains("_betti.rs")
            }) {
                fs::write(path, content)?;
                println!("📝 Generated file: {}", path.display());
            }
            
            // Run if requested
            if run {
                println!("🚀 Running Betti RDL executable...");
                
                let start_time = std::time::Instant::now();
                let telemetry_result = backend.execute(&output)
                    .map_err(|e| anyhow::anyhow!("Execution failed: {}", e))?;
                let execution_time = start_time.elapsed();
                
                println!("✅ Execution completed in {:?}", execution_time);
                
                if telemetry {
                    println!("📊 Telemetry:");
                    println!("  Events processed: {}", telemetry_result.events_processed);
                    println!("  Execution time: {}ns", telemetry_result.execution_time_ns);
                    println!("  Processes: {}", telemetry_result.process_states.len());
                    
                    if !telemetry_result.process_states.is_empty() {
                        println!("  Process states:");
                        for (pid, state) in &telemetry_result.process_states {
                            println!("    Process {}: state {}", pid, state);
                        }
                    }
                }
            } else {
                println!("💡 Use --run flag to execute the generated Betti RDL workload");
            }
            
            Ok(())
        }
        
        Commands::Repl => {
            println!("Grey Programming Language REPL v0.1.0");
            println!("Type 'exit' to quit.");
            println!();
            
            let mut stdin = io::stdin();
            let mut stdout = io::stdout();
            let mut buffer = String::new();
            
            loop {
                print!("grey> ");
                stdout.flush()?;
                
                buffer.clear();
                match stdin.read_line(&mut buffer) {
                    Ok(0) => break, // EOF
                    Ok(_) => {
                        let input = buffer.trim();
                        
                        if input.is_empty() {
                            continue;
                        }
                        
                        if input == "exit" {
                            break;
                        }
                        
                        // Try to compile the input
                        match compile(input) {
                            Ok(_) => println!("✅ Valid expression"),
                            Err(e) => println!("❌ Error: {}", e),
                        }
                    }
                    Err(e) => {
                        println!("Error reading input: {}", e);
                        break;
                    }
                }
            }
            
            println!("Goodbye!");
            Ok(())
        }
    }
}