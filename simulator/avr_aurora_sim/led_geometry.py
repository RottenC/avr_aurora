LED_COUNT=56
LEFT_RANGE=range(0,23); FRONT_RANGE=range(23,33); RIGHT_RANGE=range(33,56)

def physical_coordinates() -> list[tuple[int,int]]:
    coords=[None]*LED_COUNT
    for i in LEFT_RANGE: coords[i]=(0,i)          # rear to front: y increases
    for i in FRONT_RANGE: coords[i]=(i-22,23)    # left to right: x increases
    for i in RIGHT_RANGE: coords[i]=(11,55-i)    # front to rear: y decreases
    return coords # type: ignore
