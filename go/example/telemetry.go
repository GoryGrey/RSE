package main

import (
	"fmt"

	"github.com/betti-labs/betti-rdl"
)

func main() {
	kernel := bettirdl.NewKernel()
	defer kernel.Close()

	kernel.SpawnProcess(0, 0, 0)
	kernel.InjectEvent(0, 0, 0, 1)
	kernel.Run(100)

	tel := kernel.GetTelemetry()
	fmt.Printf("TELEMETRY,%d,%d,%d,%d\n", tel.EventsProcessed, tel.CurrentTime, tel.ProcessCount, tel.MemoryUsed)
}
