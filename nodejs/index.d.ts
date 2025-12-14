// TypeScript definitions for betti-rdl

export class Kernel {
    constructor();

    /**
     * Spawn a process at spatial coordinates (x, y, z)
     */
    spawnProcess(x: number, y: number, z: number): void;

    /**
     * Inject an event at coordinates with value
     */
    injectEvent(x: number, y: number, z: number, value: number): void;

    /**
     * Run computation for up to maxEvents
     */
    run(maxEvents: number): void;

    /**
     * Get number of events processed
     */
    getEventsProcessed(): number;

    /**
     * Get current logical time
     */
    getCurrentTime(): number;

    /**
     * Get number of active processes
     */
    getProcessCount(): number;

    /**
     * Get accumulated state for a process
     */
    getProcessState(pid: number): number;
}
