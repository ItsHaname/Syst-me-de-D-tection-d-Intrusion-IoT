#-------------------------------------------------
#                                                #
#               by : Hanane                      #
#------------------------------------------------#
iptables -F
#I should keep SSH access first :)
iptables -A INPUT -p tcp --dport 22 -s 192.168.50.4 -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -s 192.168.11.101 -j ACCEPT
#lo
iptables -A INPUT -i lo -j ACCEPT 
#MQTT
iptables -A INPUT -p tcp --dport 1883 -j ACCEPT 
#4. Bloquer l'attaquant
#Concept : quand le moteur C/C++ détecte une attaque, 
#il ajoute dynamiquement une règle qui bloque l'IP de l'attaquant.
#C'est la règle qui s'ajoute automatiquement pendant l'exécution.
