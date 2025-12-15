//! Grey Compiler CLI - Minimal Version
//! 
//! Command-line interface for the Grey programming language compiler.

use clap::{Parser, Subcommand};
use grey_lang::compile;
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