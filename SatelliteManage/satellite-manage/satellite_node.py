import ephem
import time
import docker_client
from datetime import datetime

satellites = []


def worker(i: int, total: int, pool_num: int, res):
    unit = ((total - 1) // pool_num) + 1
    range_start = i * unit
    range_end = min(total, (i + 1) * unit)
    while True:
        now = datetime.utcnow()
        for i in range(range_start, range_end):
            index_base = 3 * i
            res[index_base], res[index_base + 1], res[index_base + 2] = satellites[i].get_next_position(now)
        time.sleep(1)


class SatelliteNode:

    def __init__(self, tle_info: tuple, node_id: str, container_id: str):
        print(tle_info)
        self.satellite = ephem.readtle(tle_info[0], tle_info[1], tle_info[2])
        self.node_id = node_id
        self.container_id = container_id
        self.topo = []
        self.host_ip = ''
        self.subnet_ip = {}
        satellites.append(self)

    def get_next_position(self, time_now):
        '''
        Parameter time_str is time.strftime('%Y/%m/%d %H:%M:%S', ${time})
        Return Value: dec, ra, earth distance
        '''
        ephem_time = ephem.Date(time_now)
        self.satellite.compute(ephem_time)
        return self.satellite.sublat, self.satellite.sublong, self.satellite.elevation
