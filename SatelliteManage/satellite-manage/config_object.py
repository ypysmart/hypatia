from configparser import ConfigParser
from typing import Optional

class Config:
    def __init__(self, filepath: str):
        self.SatelliteConfigPath = "./satellites.json"
        self.UDPPort = '30000'
        self.MonitorImageName = ""  # 不再使用镜像，改为进程
        self.TrafficControlImageName = ""  # 移除
        self.Delay = "0ms"
        self.Loss = "0%"
        self.Limit = "1Gbps"
        self.Corrupt = "0%"
        self.QuaggaZebraPath = "/usr/sbin/zebra"  # 新增：Quagga zebra路径
        self.QuaggaOspfdPath = "/usr/sbin/ospfd"  # 新增：Quagga ospfd路径
        self.QuaggaConfDir = "/etc/quagga"  # 新增：Quagga配置文件目录
        parser = ConfigParser()
        parser.read(filepath, encoding='UTF-8')

        if parser.has_option("Satellite", "SatelliteConfigPath"):
            self.SatelliteConfigPath = parser["Satellite"]["SatelliteConfigPath"]

        if parser.has_option("Network", "UDPPort"):  # 改为Network部分
            self.UDPPort = parser["Network"]["UDPPort"]

        if parser.has_option("Traffic Control", "Delay"):
            self.Delay = parser["Traffic Control"]["Delay"]
        if parser.has_option("Traffic Control", "Loss"):
            self.Loss = parser["Traffic Control"]["Loss"]
        if parser.has_option("Traffic Control", "Limit"):
            self.Limit = parser["Traffic Control"]["Limit"]
        if parser.has_option("Traffic Control", "Corrupt"):
            self.Corrupt = parser["Traffic Control"]["Corrupt"]

        # 新增Quagga配置选项（如果config.ini有）
        if parser.has_option("Quagga", "ZebraPath"):
            self.QuaggaZebraPath = parser["Quagga"]["ZebraPath"]
        if parser.has_option("Quagga", "OspfdPath"):
            self.QuaggaOspfdPath = parser["Quagga"]["OspfdPath"]
        if parser.has_option("Quagga", "ConfDir"):
            self.QuaggaConfDir = parser["Quagga"]["ConfDir"]