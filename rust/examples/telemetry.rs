use betti_rdl::Kernel;

fn main() {
    let mut kernel = Kernel::new();
    kernel.spawn_process(0, 0, 0);
    kernel.inject_event(0, 0, 0, 1);
    kernel.run(100);

    let tel = kernel.get_telemetry();
    println!(
        "TELEMETRY,{},{},{},{}",
        tel.events_processed, tel.current_time, tel.process_count, tel.memory_used
    );
}
