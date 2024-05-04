# RocketMax

Ce plugin est toujours en développement. 
Il permet d'envoyer a un serveur externe par défaut [histo.tellebma.fr](https://rocketmax.tellebma.fr/) les données de fin de partie Victoire/défaite, MMR gagngé, etc

L'interface front est également toujours en développement [Github front](https://github.com/tellebma/RocketMax-FrontEnd)


![page principale, affichage global du compte](https://github.com/tellebma/RocketMax-FrontEnd/blob/main/doc/main_page.png)


# Release 

see: [Release TAG 2.0.0](https://github.com/tellebma/RocketMax-Plugin/releases/tag/2.0.0)  

Vous pouvez trouver le fichier .dll du plugin a tout moment de son développement dans plugin/RocketMax.dll
```ps
echo 'plugin load rocketmax' >> C:\Users\%username%\AppData\Roaming\bakkesmod\bakkesmod\cfg\plugin.cfg
```

Il est possible de changer le serveur target pour cela modifier la variable API_ENDPOINT dans le fichier RocketMax.cpp
```cpp
#define API_ENDPOINT "http://localhost:5000"
```




