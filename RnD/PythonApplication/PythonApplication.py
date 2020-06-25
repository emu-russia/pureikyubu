# Tutorial: Work with Python in Visual Studio
# https://docs.microsoft.com/ru-ru/visualstudio/python/tutorial-working-with-python-in-visual-studio-step-01-create-project?view=vs-2019

import sys
from math import cos, radians

print ("Hello Python")

for i in range(360):
	print(cos(radians(i)))
