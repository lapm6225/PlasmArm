
import numpy as np


L1=150
L2=150
choice=int(input("direct:1 \nreverse:2\n"))
if choice==1:
    theta1=np.deg2rad(float(input("theta1: ")))#en radian
    theta2=np.deg2rad(float(input("theta2: ")))

    x=L1*np.cos(theta1)+L2*np.cos(theta1+theta2)
    y=L1*np.sin(theta1)+L2*np.sin(theta1+theta2)
    print("(",x,",",y,")")
elif 2:
    x=float(input("x: "))
    y=float(input("y: "))

    theta2=np.arccos((x**2+y**2-L1**2-L2**2)/(2*L1*L2))
    theta1=np.arctan2(y,x)-np.arctan2(L2*np.sin(theta2),L1+L2*np.cos(theta2))
    print("theta1: ",np.rad2deg(theta1),"theta2: ",np.rad2deg(theta2))

    theta2=-np.arccos((x**2+y**2-L1**2-L2**2)/(2*L1*L2))
    theta1=np.arctan2(y,x)+np.arctan2(L2*np.sin(theta2),L1+L2*np.cos(theta2))
    print("theta1: ",np.rad2deg(theta1),"theta2: ",np.rad2deg(theta2))
