import unittest
import betti_rdl

class TestBettiRDL(unittest.TestCase):
    def test_run_returns_count(self):
        kernel = betti_rdl.Kernel()
        kernel.spawn_process(0, 0, 0)
        # Inject 3 events at x=9 so they don't propagate (next_x=10 !< 10)
        kernel.inject_event(9, 0, 0, 1)
        kernel.inject_event(9, 0, 0, 2)
        kernel.inject_event(9, 0, 0, 3)
        
        # Run with max 100
        count = kernel.run(100)
        
        # Should process 3 events
        self.assertEqual(count, 3)
        self.assertEqual(kernel.events_processed, 3)

    def test_run_limit(self):
        kernel = betti_rdl.Kernel()
        kernel.spawn_process(0, 0, 0)
        # Inject 10 events
        for i in range(10):
            kernel.inject_event(9, 0, 0, i)
            
        # Run with max 5
        count = kernel.run(5)
        
        self.assertEqual(count, 5)
        self.assertEqual(kernel.events_processed, 5)
        
        # Run remaining
        count2 = kernel.run(10)
        self.assertEqual(count2, 5)
        self.assertEqual(kernel.events_processed, 10)

if __name__ == '__main__':
    unittest.main()
