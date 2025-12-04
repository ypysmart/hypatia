from configparser import ConfigParser
from typing import Optional


class Config:

    def __init__(self, filepath: str):
        self.SatelliteConfigPath = "./satellites.json"
        self.DockerHostIP = "172.17.0.1"
        self.DockerImageName = "ubuntu:18.04"
        self.UDPPort = '30000'
        self.MonitorImageName = ""
        self.TrafficControlImageName = ""
        self.Delay = "0ms"
        self.Loss = "0%"
        self.Limit = "1Gbps"
        self.Corrupt = "0%"
        parser = ConfigParser()
        parser.read(filepath, encoding='UTF-8')

        if parser.has_option("Satellite", "SatelliteConfigPath"):
            self.SatelliteConfigPath = parser["Satellite"]["SatelliteConfigPath"]

        if parser.has_option("Docker", "DockerHostIP"):
            self.DockerHostIP = parser["Docker"]["DockerHostIP"]
        if parser.has_option("Docker", "ImageName"):
            self.DockerImageName = parser["Docker"]["ImageName"]
        if parser.has_option("Docker", "UDPPort"):
            self.UDPPort = parser["Docker"]["UDPPort"]
        if parser.has_option("Docker", "MonitorImageName"):
            self.MonitorImageName = parser["Docker"]["MonitorImageName"]
        if parser.has_option("Docker", "TrafficControlImageName"):
            self.TrafficControlImageName = parser["Docker"]["TrafficControlImageName"]

        if parser.has_option("Traffic Control", "Delay"):
            self.Delay = parser["Traffic Control"]["Delay"]
        if parser.has_option("Traffic Control", "Loss"):
            self.Loss = parser["Traffic Control"]["Loss"]
        if parser.has_option("Traffic Control", "Limit"):
            self.Limit = parser["Traffic Control"]["Limit"]
        if parser.has_option("Traffic Control", "Corrupt"):
            self.Corrupt = parser["Traffic Control"]["Corrupt"]
