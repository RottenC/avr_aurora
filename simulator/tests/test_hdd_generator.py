from avr_aurora_sim.hdd_generator import HddGenerator,HddMode

def seq(seed,mode):
    g=HddGenerator(seed,mode); return [g.update(20,False) for _ in range(50)]

def test_deterministic_same_seed(): assert seq(7,HddMode.RANDOM)==seq(7,HddMode.RANDOM)
def test_different_seeds(): assert seq(7,HddMode.RANDOM)!=seq(8,HddMode.RANDOM)
def test_manual_mode():
    g=HddGenerator(1,HddMode.MANUAL); assert g.update(20,True)==(True,[]); assert g.update(20,False)==(False,[])
def test_activity_modes_emit_edges():
    for mode in (HddMode.LIGHT,HddMode.MEDIUM,HddMode.HEAVY,HddMode.RANDOM):
        g=HddGenerator(3,mode); assert any(edges for _,edges in (g.update(20,False) for _ in range(200)))
