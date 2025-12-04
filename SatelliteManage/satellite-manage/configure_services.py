import logging
import time
import requests
import json

port = 8080


def init_monitor(image_name: str, client, udp_port: int):
    try:
        # 检查是否已存在Monitor容器，如果存在则移除
        for container in client.client.containers.list(all=True):
            if container.name == "satellite-monitor":
                logging.info("移除已存在的Monitor容器")
                container.remove(force=True)
                
        # 创建新的Monitor容器，不使用host网络模式
        container = client.client.containers.run(
            image_name, 
            detach=True, 
            name="satellite-monitor",
            environment=['UDP_PORT=' + str(udp_port)],
            ports={'%d/tcp' % port: port}  # 保留端口绑定，不使用host网络模式
        )
        
        # 等待容器启动
        time.sleep(1)
        
        # 检查容器是否运行
        container.reload()
        if container.status != "running":
            logging.error(f"Monitor容器状态: {container.status}")
            logs = container.logs().decode('utf-8')
            logging.error(f"Monitor容器日志: {logs}")
            raise Exception("Monitor容器未能成功运行")
            
        logging.info("Monitor容器已成功启动")
        return container.id
    except Exception as e:
        logging.error(f"初始化Monitor失败: {e}")
        raise

def set_monitor(raw_payload: list):
    try:
        payload = {
            'total': len(raw_payload),
            'items': raw_payload
        }
        
        # 尝试不同的连接方式
        # 1. 使用localhost
        try:
            response = requests.post(
                url=f'http://localhost:{port}/api/satellite/list',
                json=payload,  # 使用json参数自动设置Content-Type
                timeout=5
            )
            if response.status_code == 200:
                return True
        except Exception as e:
            print(f"使用localhost连接失败: {e}")
            
        # 2. 使用容器IP
        try:
            import subprocess
            result = subprocess.run(
                ['docker', 'inspect', '-f', '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}', 'satellite-monitor'],
                capture_output=True, text=True
            )
            container_ip = result.stdout.strip()
            
            response = requests.post(
                url=f'http://{container_ip}:{port}/api/satellite/list',
                json=payload,
                timeout=5
            )
            if response.status_code == 200:
                return True
        except Exception as e:
            print(f"使用容器IP连接失败: {e}")
            
        return False
            
    except Exception as e:
        print(f"设置Monitor时出错: {e}")
        return False
