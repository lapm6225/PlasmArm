import asyncio
import websockets
import json
import ezdxf

class RobotClient:
    def __init__(self, ip, buffer_size=10):
        self.uri = f"ws://{ip}/ws"
        self.websocket = None
        # Le sémaphore gère les crédits. On ne peut pas envoyer plus que buffer_size
        self.credits = asyncio.BoundedSemaphore(buffer_size)
        self.connected = False
        self.position = {'x': 0, 'y': 0}
        self.is_moving = False

    async def connect(self):
        """Établit la connexion et lance l'écouteur en tâche de fond"""
        try:
            self.websocket = await websockets.connect(self.uri)
            self.connected = True
            print("Connecté au Robot.")
            # On lance l'écouteur (Receiver) sans bloquer le reste
            asyncio.create_task(self._listener())
        except Exception as e:
            print(f"Échec connexion: {e}")

    async def _listener(self):
        """Écoute en permanence les messages de l'ESP32"""
        try:
            async for message in self.websocket:
                data = json.loads(message)
                
                # 1. Gestion du ACK (Contrôle de flux)
                if data.get("type") == "ACK":
                    # On libère un crédit, ce qui débloquera la fonction send_command
                    try:
                        self.credits.release()
                    except ValueError:
                        pass # Sémaphore déjà plein (cas rare init)

                # 2. Gestion du Feedback de position
                elif "x" in data:
                    self.position = {'x': data['x'], 'y': data['y']}
                    self.is_moving = data.get('isMoving', False)
                    # print(f"Pos: {self.position}") # Debug optionnel

        except websockets.exceptions.ConnectionClosed:
            self.connected = False
            print("Connexion perdue.")

    async def send_command(self, cmd_dict):
        """Envoie une commande de manière sûre (bloque si buffer plein)"""
        if not self.connected:
            print("Erreur: Non connecté")
            return

        # C'est ICI que le context switch se fait !
        # Si plus de crédits, on attend (await) que le listener reçoive un ACK.
        await self.credits.acquire() 
        
        await self.websocket.send(json.dumps(cmd_dict))

    async def upload_dxf_file(self, filename):
        """Fonction haut niveau pour traiter un fichier"""
        print(f"Début envoi {filename}...")
        try:
            doc = ezdxf.readfile(filename)
            msp = doc.modelspace()
            
            x_end, y_end = 0, 0
            
            for e in msp:
                if e.dxftype() == 'LINE':
                    x_start, y_start = e.dxf.start[0], e.dxf.start[1]
                    
                    # Logique de déplacement (similaire à ton code)
                    if abs(x_start - x_end) > 0.001 or abs(y_start - y_end) > 0.001:
                        await self.send_command({"type":"TOOL","state":False,"z":5.0})
                        await self.send_command({"type": "MOVE_TO", "x": x_start, "y": y_start})
                        await self.send_command({"type":"TOOL","state":False,"z":0.0})
                    
                    x_end, y_end = e.dxf.end[0], e.dxf.end[1]
                    await self.send_command({"type": "MOVE_TO", "x": x_end, "y": y_end})
            
            # Fin du fichier
            await self.send_command({"type":"TOOL","state":False,"z":5.0})
            print("Fichier envoyé avec succès !")
            
        except Exception as e:
            print(f"Erreur DXF: {e}")

# --- Exemple d'utilisation (Simulation d'un bouton) ---

robot = RobotClient(ip="192.168.17.151", buffer_size=10)

async def on_button_click():
    """Ceci serait la fonction appelée par ton bouton d'interface"""
    if not robot.connected:
        await robot.connect()
    
    # Cet appel ne bloquera pas l'UI car il utilise des await
    await robot.upload_dxf_file("drawing_simple.dxf")

# Simulation du démarrage
if __name__ == "__main__":
    while True:
        if input("send file [y/n]")=="y":
            asyncio.run(on_button_click())