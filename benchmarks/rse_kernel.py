import random
import time
import csv
import sys
import os

# RSE Kernel - Python Port for Scientific Verification
# Goals:
# 1. Replicate logic outside V8/Node
# 2. Verify O(1) memory scaling in a reference-counted language

class SymbolicAgent:
    def __init__(self, id, symbol):
        self.id = id
        self.symbol = symbol
        self.x = 0
        self.y = 0
        self.z = 0
        self.age = 0

class ToroidalSpace:
    def __init__(self, width=32, height=32, depth=32):
        self.width = width
        self.height = height
        self.depth = depth
        self.grid = {} # Dict for sparse storage, similar to JS Map

    def add_agent(self, agent):
        self.grid[agent.id] = agent

    def move_agent(self, agent, dx, dy, dz):
        agent.x = (agent.x + dx) % self.width
        agent.y = (agent.y + dy) % self.height
        agent.z = (agent.z + dz) % self.depth
        # In a full physics engine, we'd update spatial partition here

class RSEKernel:
    def __init__(self):
        self.space = ToroidalSpace()
        self.cycle = 0
    
    def init(self, agent_count=50):
        for i in range(agent_count):
            self.spawn_agent()

    def spawn_agent(self):
        uid = f"{random.randint(0, 100000000)}"
        char = chr(65 + random.randint(0, 25))
        agent = SymbolicAgent(uid, char)
        agent.x = random.randint(0, self.space.width - 1)
        agent.y = random.randint(0, self.space.height - 1)
        agent.z = random.randint(0, self.space.depth - 1)
        self.space.add_agent(agent)

    def step(self):
        self.cycle += 1
        # Iterate over all agents
        # In Python, list(dict.values()) creates a snapshot list, consuming Memory O(N) relative to agent count
        # But relative to *Recursion Depth*, this is O(1) because N is constant.
        agents = list(self.space.grid.values())
        
        for agent in agents:
            agent.age += 1
            if random.random() > 0.8:
                dx = random.randint(-1, 1)
                dy = random.randint(-1, 1)
                dz = random.randint(-1, 1)
                self.space.move_agent(agent, dx, dy, dz)

def run_telemetry():
    print("=================================================")
    print("   RSE KERNEL // PYTHON 3 // REFERENCE PORT      ")
    print("=================================================")
    
    kernel = RSEKernel()
    kernel.init(50) # Same 50 agents as JS default
    
    TOTAL_STEPS = 50000
    SAMPLE_RATE = 10
    LOG_FILE = "rse_py_telemetry.csv"
    
    print(f"[SETUP] Logging to {LOG_FILE}")
    print(f"[RUN] Executing {TOTAL_STEPS} steps...")
    
    # Try getting memory module
    process = None
    try:
        import psutil
        process = psutil.Process(os.getpid())
    except ImportError:
        print("[WARN] 'psutil' not found. Memory logging will be approximate or skipped.")

    with open(LOG_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Step", "RSS_MB", "TimeDelta_ms"])
        
        start_time = time.time()
        last_time = start_time
        
        for step in range(1, TOTAL_STEPS + 1):
            kernel.step()
            
            if step % SAMPLE_RATE == 0:
                now = time.time()
                rss_mb = 0
                if process:
                    rss_mb = process.memory_info().rss / 1024 / 1024
                
                dt = (now - last_time) * 1000
                writer.writerow([step, f"{rss_mb:.4f}", f"{dt:.4f}"])
                last_time = now
            
            if step % 5000 == 0:
                print(f"    > Progress: {step}/{TOTAL_STEPS}")

    print("[DONE] Execution Complete.")

if __name__ == "__main__":
    run_telemetry()
