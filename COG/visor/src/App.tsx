import { Canvas, useFrame } from '@react-three/fiber'
import { OrbitControls, Stars, Text } from '@react-three/drei'
import { useEffect, useRef, useState, useMemo } from 'react'
import * as THREE from 'three'

// Wasm Type Definition
declare global {
    interface Window {
        createGenesisModule: () => Promise<any>;
    }
}

function GenesisSwarm({ wasm }: { wasm: any }) {
    const meshRef = useRef<THREE.InstancedMesh>(null!)
    const count = 1000 // Start with 1000 agents
    const tempObject = new THREE.Object3D()

    // Buffers for direct memory access
    const [positions, setPositions] = useState<Float32Array | null>(null)

    useEffect(() => {
        // Initial Color
        for (let i = 0; i < count; i++) {
            tempObject.position.set(0, 0, 0)
            tempObject.updateMatrix()
            meshRef.current.setMatrixAt(i, tempObject.matrix)
            meshRef.current.setColorAt(i, new THREE.Color(0x00ff00))
        }
        meshRef.current.instanceMatrix.needsUpdate = true
    }, [])

    useFrame((state, delta) => {
        if (wasm && meshRef.current) {
            // Run Physics Tick in C++
            wasm.tick(delta * 1.0);

            // Get Data Pointer
            const agentPos = wasm.get_agent_positions();

            // Render Loop
            for (let i = 0; i < count; i++) {
                const x = agentPos.get(i * 3);
                const y = agentPos.get(i * 3 + 1);
                const z = agentPos.get(i * 3 + 2);

                tempObject.position.set(x, y, z);
                // Scale based on energy/speed
                const speed = Math.sqrt(x * x + y * y + z * z);
                const s = Math.min(1.0, 0.1 + (speed / 50.0));
                tempObject.scale.set(s, s, s);

                tempObject.updateMatrix();
                meshRef.current.setMatrixAt(i, tempObject.matrix);
                const hue = (speed / 100.0) % 1.0;
                meshRef.current.setColorAt(i, new THREE.Color().setHSL(hue, 1.0, 0.5));
            }
            agentPos.delete();
        } else if (meshRef.current) {
            // JS FALLBACK PHYSICS (If Wasm fails to load)
            const time = state.clock.getElapsedTime();
            for (let i = 0; i < count; i++) {
                // Galaxy Spiral Logic
                const angle = time * 0.1 + (i * 0.1);
                const radius = 20 + Math.sin(time * 0.2 + i) * 10;
                const x = Math.cos(angle) * radius;
                const y = Math.sin(time * 0.3 + i * 0.5) * 5;
                const z = Math.sin(angle) * radius;

                tempObject.position.set(x, y, z);
                tempObject.scale.set(0.2, 0.2, 0.2);
                tempObject.updateMatrix();
                meshRef.current.setMatrixAt(i, tempObject.matrix);
                meshRef.current.setColorAt(i, new THREE.Color().setHSL(i / count, 1, 0.5));
            }
        }
        if (meshRef.current) {
            meshRef.current.instanceMatrix.needsUpdate = true;
            meshRef.current.instanceColor.needsUpdate = true;
        }
    })

    return (
        <instancedMesh ref={meshRef} args={[null, null, count]}>
            <boxGeometry args={[0.2, 0.2, 0.2]} />
            <meshBasicMaterial />
        </instancedMesh>
    )
}

function Planets({ wasm }: { wasm: any }) {
    // Mock planets if WASM not loaded, else read from WASM
    return (
        <group>
            {/* The Sun */}
            <mesh position={[0, 0, 0]}>
                <sphereGeometry args={[10, 32, 32]} />
                <meshStandardMaterial emissive="#ffdd00" emissiveIntensity={2} color="#ffdd00" />
            </mesh>
            {/* Blue Giant */}
            <mesh position={[60, 0, 0]}>
                <sphereGeometry args={[5, 32, 32]} />
                <meshStandardMaterial color="#0000ff" />
            </mesh>
            {/* Red Dwarf */}
            <mesh position={[-40, 40, 0]}>
                <sphereGeometry args={[8, 32, 32]} />
                <meshStandardMaterial color="#ff0000" />
            </mesh>
        </group>
    )
}

export default function App() {
    const [wasm, setWasm] = useState<any>(null);
    const [error, setError] = useState<string>("");

    // Load Wasm
    useEffect(() => {
        const script = document.createElement('script');
        script.src = "/genesis.js";
        script.async = true;
        script.onload = async () => {
            try {
                const Module = await window.createGenesisModule();
                const universe = new Module.Universe(1000); // 1000 Agents
                setWasm(universe);
                console.log("🌌 WASM Universe Initialized");
            } catch (e: any) {
                setError(e.message);
            }
        };
        script.onerror = () => {
            setError("Wasm binary not found. Run 'build_wasm.ps1'!");
        };
        document.body.appendChild(script);
    }, []);

    return (
        <div style={{ width: '100vw', height: '100vh', background: 'black' }}>
            <Canvas camera={{ position: [0, 50, 80], fov: 60 }}>
                <OrbitControls autoRotate autoRotateSpeed={0.5} />
                <Stars radius={200} depth={50} count={5000} factor={4} saturation={0} fade speed={1} />

                {wasm && <GenesisSwarm wasm={wasm} />}
                <Planets wasm={wasm} />

                <ambientLight intensity={0.2} />
                <pointLight position={[0, 0, 0]} intensity={2} distance={200} />
            </Canvas>

            <div style={{ position: 'absolute', top: 20, left: 20, color: '#0f0', fontFamily: 'monospace', pointerEvents: 'none' }}>
                <h1 style={{ margin: 0 }}>PROJECT GENESIS</h1>
                <p style={{ margin: 0, color: 'cyan' }}>Status: {wasm ? "RUNNING (WASM)" : "CONNECTING..."}</p>
                {error && <p style={{ color: 'red', background: 'rgba(0,0,0,0.8)', padding: '10px' }}>ERROR: {error}</p>}
            </div>
        </div>
    )
}
