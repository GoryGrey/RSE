// TypeScript definitions for betti-rdl

export interface Telemetry {
    events_processed: number;
    current_time: number;
    process_count: number;
    memory_used: number;
}

export class Kernel {
    constructor();

    /**
     * Spawn a process at spatial coordinates (x, y, z)
     */
    spawnProcess(x: number, y: number, z: number): void;

    /**
     * snake_case alias for spawnProcess
     */
    spawn_process(x: number, y: number, z: number): void;

    /**
     * Inject an event at coordinates with value
     */
    injectEvent(x: number, y: number, z: number, value: number): void;

    /**
     * snake_case alias for injectEvent
     */
    inject_event(x: number, y: number, z: number, value: number): void;

    /**
     * Run computation for up to maxEvents.
     * Returns the number of events processed in this run.
     */
    run(maxEvents: number): number;

    /**
     * Get lifetime number of events processed
     */
    getEventsProcessed(): number;

    /**
     * snake_case alias for getEventsProcessed
     */
    get_events_processed(): number;

    /**
     * Get current logical time
     */
    getCurrentTime(): number;

    /**
     * snake_case alias for getCurrentTime
     */
    get_current_time(): number;

    /**
     * Get number of active processes
     */
    getProcessCount(): number;

    /**
     * snake_case alias for getProcessCount
     */
    get_process_count(): number;

    /**
     * Get runtime telemetry
     */
    getTelemetry(): Telemetry;

    /**
     * snake_case alias for getTelemetry
     */
    get_telemetry(): Telemetry;

    /**
     * Get accumulated state for a process
     */
    getProcessState(pid: number): number;

    /**
     * snake_case alias for getProcessState
     */
    get_process_state(pid: number): number;
}
