import asyncio
import websockets
import json
import random

async def robot_controller():
    # Remplace par l'IP affichée dans ton moniteur série Arduino
    uri = "ws://192.168.1.xx/ws" 
    
    try:
        async with websockets.connect(uri) as websocket:
            print("Connecté au bras SCARA !")

            # 1. Exemple d'envoi d'une commande MOVE_TO
            move_cmd = {
                "type": "MOVE_TO",
                "x": 150.5,
                "y": 75.0,
                "speed": 100
            }
            await websocket.send(json.dumps(move_cmd))
            print(f"Commande envoyée : {move_cmd}")

            test_i=0
            # 2. Boucle pour recevoir les mises à jour (broadcastStatus)
            while True:
                response = await websocket.recv()
                data = json.loads(response)
                test_i+=1
                if test_i>5:
                    test_i=0
                    move_cmd = {
                        "type": "MOVE_TO",
                        "x": random.randrange(0,150),
                        "y": random.randrange(-100,100),
                        "speed": 100
                    }
                    await websocket.send(json.dumps(move_cmd))
                    print(f"Commande envoyée : {move_cmd}")                

                # Affiche la position reçue de l'ESP32 (broadcastStatus)
                print(f"Position Robot -> X: {data['x']}, Y: {data['y']}, Moving: {data['isMoving']}")

    except Exception as e:
        print(f"Erreur de connexion : {e}")

# Lancer l'application
asyncio.get_event_loop().run_until_complete(robot_controller())