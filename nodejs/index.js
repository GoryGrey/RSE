const native = require('./build/Release/betti_rdl');

class Kernel {
    constructor() {
        this._native = new native.Kernel();
    }

    // camelCase API (native)
    spawnProcess(x, y, z) {
        return this._native.spawnProcess(x, y, z);
    }

    injectEvent(x, y, z, value) {
        return this._native.injectEvent(x, y, z, value);
    }

    run(maxEvents) {
        return this._native.run(maxEvents);
    }

    getEventsProcessed() {
        return this._native.getEventsProcessed();
    }

    getCurrentTime() {
        return this._native.getCurrentTime();
    }

    getProcessCount() {
        return this._native.getProcessCount();
    }

    getTelemetry() {
        return this._native.getTelemetry();
    }

    getProcessState(pid) {
        return this._native.getProcessState(pid);
    }

    // snake_case aliases (for parity with other bindings)
    spawn_process(x, y, z) {
        return this.spawnProcess(x, y, z);
    }

    inject_event(x, y, z, value) {
        return this.injectEvent(x, y, z, value);
    }

    get_events_processed() {
        return this.getEventsProcessed();
    }

    get_current_time() {
        return this.getCurrentTime();
    }

    get_process_count() {
        return this.getProcessCount();
    }

    get_telemetry() {
        return this.getTelemetry();
    }

    get_process_state(pid) {
        return this.getProcessState(pid);
    }
}

module.exports = { Kernel };
