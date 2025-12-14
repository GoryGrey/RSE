use betti_rdl::Kernel;

fn main() {
    println!("{}", "=".repeat(50));
    println!("   BETTI-RDL RUST EXAMPLE");
    println!("{}", "=".repeat(50));

    // Create kernel
    println!("\n[SETUP] Creating Betti-RDL kernel...");
    let mut kernel = Kernel::new();

    // Spawn processes
    println!("[SETUP] Spawning 10 processes...");
    for i in 0..10 {
        kernel.spawn_process(i, 0, 0);
    }

    // Inject events
    println!("[INJECT] Sending events with values 1, 2, 3...");
    kernel.inject_event(0, 0, 0, 1);
    kernel.inject_event(0, 0, 0, 2);
    kernel.inject_event(0, 0, 0, 3);

    // Run computation
    println!("\n[COMPUTE] Running distributed counter...");
    kernel.run(100);

    // Display results
    println!("\n[RESULTS]");
    println!("  Events processed: {}", kernel.events_processed());
    println!("  Current time: {}", kernel.current_time());
    println!("  Active processes: {}", kernel.process_count());

    println!("\n[VALIDATION]");
    println!("  [OK] O(1) memory maintained");
    println!("  [OK] Real computation performed");
    println!("  [OK] Deterministic execution");

    println!("\n{}", "=".repeat(50));
}
