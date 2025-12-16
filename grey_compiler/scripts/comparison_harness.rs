//! Betti RDL Comparison Harness
//!
//! This directory is kept for historical reasons.
//!
//! The production harness is now implemented as a proper Cargo crate so it can be
//! exercised via integration tests:
//!
//! - Harness crate: `grey_compiler/crates/grey_harness`
//! - Binary: `cargo run -p grey_harness --bin grey_compare_sir -- --max-events 1000 --seed 42`
//!
//! See `grey_compiler/README.md` for the full end-to-end flow.

fn main() {
    eprintln!(
        "This harness has moved. Run: cargo run -p grey_harness --bin grey_compare_sir -- --help"
    );
}
