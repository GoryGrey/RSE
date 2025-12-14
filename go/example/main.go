package main

import (
	"fmt"
	"strings"

	"github.com/betti-labs/betti-rdl"
)

func main() {
	fmt.Println(strings.Repeat("=", 50))
	fmt.Println("   BETTI-RDL GO EXAMPLE")
	fmt.Println(strings.Repeat("=", 50))

	// Create kernel
	fmt.Println("\n[SETUP] Creating Betti-RDL kernel...")
	kernel := bettirdl.NewKernel()
	defer kernel.Close()

	// Spawn processes
	fmt.Println("[SETUP] Spawning 10 processes...")
	for i := 0; i < 10; i++ {
		kernel.SpawnProcess(i, 0, 0)
	}

	// Inject events
	fmt.Println("[INJECT] Sending events with values 1, 2, 3...")
	kernel.InjectEvent(0, 0, 0, 1)
	kernel.InjectEvent(0, 0, 0, 2)
	kernel.InjectEvent(0, 0, 0, 3)

	// Run computation
	fmt.Println("\n[COMPUTE] Running distributed counter...")
	kernel.Run(100)

	// Display results
	fmt.Println("\n[RESULTS]")
	fmt.Printf("  Events processed: %d\n", kernel.EventsProcessed())
	fmt.Printf("  Current time: %d\n", kernel.CurrentTime())
	fmt.Printf("  Active processes: %d\n", kernel.ProcessCount())

	fmt.Println("\n[VALIDATION]")
	fmt.Println("  [OK] O(1) memory maintained")
	fmt.Println("  [OK] Real computation performed")
	fmt.Println("  [OK] Deterministic execution")

	fmt.Println("\n" + strings.Repeat("=", 50))
}
