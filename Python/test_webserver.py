import asyncio
import json
import unittest
import websockets

# Paramètres de connexion de base
ESP32_IP = "192.168.4.1"
WS_PORT = 80
WS_PATH = "/ws"
WS_URI = f"ws://{ESP32_IP}:{WS_PORT}{WS_PATH}"

class TestESP32WebServer(unittest.IsolatedAsyncioTestCase):
    """
    Test unitaire d'intégration du WebServer WebSocket sur ESP32.
    Nécessite que l'ESP32 soit allumé et en mode Point d'Accès (192.168.4.1).
    """

    async def asyncSetUp(self):
        """S'exécute avant chaque test. Établit la connexion."""
        try:
            self.ws = await asyncio.wait_for(
                websockets.connect(
                    WS_URI, 
                    ping_interval=20, 
                    ping_timeout=10, 
                    close_timeout=5
                ),
                timeout=5.0
            )
        except Exception as e:
            self.fail(f"Impossible de se connecter à l'ESP32 sur {WS_URI}. Assurez-vous d'être connecté au réseau Wi-Fi 'PlasmArm_ESP32'. Erreur: {e}")

        # Lors de la connexion, l'ESP32 doit envoyer un message "BUFFER"
        try:
            raw_msg = await asyncio.wait_for(self.ws.recv(), timeout=2.0)
            data = json.loads(raw_msg)
            self.assertEqual(data.get("type"), "BUFFER", "Le premier message doit être de type BUFFER")
            self.assertIn("cmdFree", data, "Le message BUFFER doit contenir cmdFree")
            self.initial_cmd_free = data["cmdFree"]
            self.handled_offset = data.get("handled", 0)
        except Exception as e:
            self.fail(f"Erreur lors de la réception du message BUFFER initial: {e}")

    async def asyncTearDown(self):
        """S'exécute après chaque test. Ferme la connexion."""
        if hasattr(self, 'ws') and self.ws:
            await self.ws.close()

    async def test_single_command_flow(self):
        """Teste l'envoi d'une seule commande et la réception d'un ACK."""
        cmd = {
            "type": "MOVE_TO",
            "x": 10.0,
            "y": 10.0,
            "z": 0.0,
            "speed": 50.0,
            "tool": False
        }
        await self.ws.send(json.dumps(cmd))

        # On attend l'ACK en ignorant les éventuels messages STATUS périodiques
        data = None
        while True:
            raw_msg = await asyncio.wait_for(self.ws.recv(), timeout=2.0)
            data = json.loads(raw_msg)
            if data.get("type") == "ACK":
                break
            elif data.get("type") == "ERROR":
                self.fail(f"Reçu une erreur inattendue: {data.get('msg')}")

        self.assertEqual(data.get("type"), "ACK", "Le serveur aurait dû répondre avec un ACK pour MOVE_TO")
        self.assertIn("cmdFree", data)
        # L'espace libre ne devrait pas dépasser la taille max de la file (habituellement 30)
        self.assertLessEqual(data["cmdFree"], self.initial_cmd_free)

    async def test_robust_flow_control_burst(self):
        """
        Teste le mécanisme de flow control robuste en inondant la connexion avec plus de 
        commandes que la file (ex: 50 commandes alors que la file est max 30).
        """
        burst_size = 50
        
        in_flight = 0
        cmd_free = self.initial_cmd_free
        handled_offset = getattr(self, "handled_offset", 0)
        
        sent_count = 0
        ack_count = 0
        handled_by_server = 0

        # Tâche asynchrone pour lire les réponses sans bloquer l'envoi
        async def receiver_task():
            nonlocal ack_count, in_flight, cmd_free, handled_by_server
            while handled_by_server < burst_size:
                try:
                    raw_msg = await asyncio.wait_for(self.ws.recv(), timeout=5.0)
                    data = json.loads(raw_msg)
                    
                    if "handled" in data:
                        # Calcul exact des messages réellement "en vol"
                        handled_by_server = data["handled"] - handled_offset
                        in_flight = max(0, sent_count - handled_by_server)
                        
                    if data.get("type") == "ACK":
                        ack_count += 1
                        
                    if "cmdFree" in data:
                        cmd_free = data["cmdFree"]
                except asyncio.TimeoutError:
                    break

        recv_future = asyncio.create_task(receiver_task())

        # Envoi en masse en utilisant notre algorithme de limitation in-flight
        while sent_count < burst_size:
            virtual_free = cmd_free - in_flight
            
            if virtual_free > 0:
                cmd = {
                    "type": "MOVE_TO",
                    "x": float(sent_count % 100),
                    "y": float(sent_count % 100),
                    "z": 0.0,
                    "speed": 100.0,
                    "tool": False
                }
                await self.ws.send(json.dumps(cmd))
                sent_count += 1
                in_flight += 1 # Prévision immédiate
                
                # Petit délai pour laisser le temps au réseau
                await asyncio.sleep(0.005)
            else:
                # Si le buffer virtuel est plein, on attend
                await asyncio.sleep(0.05)

        # Attendre que tout soit traité
        await asyncio.wait_for(recv_future, timeout=10.0)

        # L'ESP32 ne peut pas toujours envoyer 50 ACKs de suite à cause des buffers TCP internes,
        # mais le compteur `handled` garantit le traitement exact!
        self.assertEqual(handled_by_server, burst_size, f"Le serveur aurait dû traiter (handled) les {burst_size} commandes.")


if __name__ == '__main__':
    unittest.main(verbosity=2)
