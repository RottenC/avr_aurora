from avr_aurora_sim.led_geometry import physical_coordinates,LED_COUNT,LEFT_RANGE,FRONT_RANGE,RIGHT_RANGE

def test_count_and_unique():
    coords=physical_coordinates(); assert len(coords)==LED_COUNT; assert len(set(coords))==LED_COUNT

def test_ranges(): assert list(LEFT_RANGE)==list(range(23)) and list(FRONT_RANGE)==list(range(23,33)) and list(RIGHT_RANGE)==list(range(33,56))
def test_direction():
    c=physical_coordinates(); assert c[0][1] < c[22][1]; assert c[23][0] < c[32][0]; assert c[33][1] > c[55][1]
