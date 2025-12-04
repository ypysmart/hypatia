import json


def parse_config(raw_json: str) -> (list, list):
    json_obj = json.loads(raw_json)
    satellites = json_obj['satellites']
    connection = json_obj['connections']
    return satellites, connection


class SatelliteConfigParser:

    def __init__(self, path: str):
        self.satellites = []
        self.connection = {}
        self.path = path
        tle_file = open(self.path, 'r')
        data = tle_file.read()
        self.satellites, self.connection = parse_config(data)

    def get_satellites(self) -> list:
        return self.satellites

    def get_connection(self) -> dict:
        return self.connection
