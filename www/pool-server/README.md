# pool-server

## MongoDB v8.0 Setup

```
curl -fsSL https://www.mongodb.org/static/pgp/server-8.0.asc | \
   sudo gpg -o /usr/share/keyrings/mongodb-server-8.0.gpg \
   --dearmor
echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/8.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-8.0.list
sudo apt-get update
sudo apt-get install -y mongodb-org
```

### Config

/etc/mongod.conf
```
net:
  port: 27017
  bindIp: 0.0.0.0
replication:
  replSetName: rs
```

`sudo systemctl restart mongod`

### Firewall

`sudo apt install ufw`

```
# ufw status
Status: active

To                         Action      From
--                         ------      ----
22/tcp                     ALLOW       Anywhere                  
Anywhere                   ALLOW       69.197.174.72             
Anywhere                   ALLOW       49.231.43.170             
Anywhere                   ALLOW       110.164.184.5             
Anywhere                   ALLOW       89.116.184.19             
27017                      DENY        Anywhere                  
22/tcp (v6)                ALLOW       Anywhere (v6)             
27017 (v6)                 DENY        Anywhere (v6)
```

### Add Member

```
$ mongosh
rs.add({host: 'mmx-si-1.mmx.network', priority: 1})
```

### Inital Setup

`mongosh` on one server after adding members:
```
rs.initiate()
```
