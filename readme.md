
这个程序会提供提币的接口，并调用cleos与node结点和keosd钱包交互。

## 下载源码
git clone https://github.com/qmarliu/tgc-currency-server

## 编译源码
`cd currency_server_helper`
+ 编译基础代码
   - 编译network代码
   cd network
   安装依赖libev
   ```
    git clone https://github.com/enki/libev.git
    cd libev
    sh autogen.sh
    ./configure && make
    sudo make install
   ```
    `make`
   - 编译utils代码
   ```
   cd utils
   make
   ```
+ 编译可执行程序
    `make`

## 配置文件config.json说明
```
{
    "debug": true,
    "process": {
        "file_limit": 1000000,
        "core_limit": 1000000000
    },
    "log": {
        "path": "/var/log/trade/currency_server_helper",
        "flag": "fatal,error,warn,info,debug,trace",
        "num": 10
    },
    "svr": {    //程序监听的端口，通过通过这个端口来接收http的调用
        "bind": [
            "tcp@0.0.0.0:8082"
        ],
        "max_pkg_size": 102400
    },
    "worker_num": 1, //工作线程数量
    "excutor": "/home/liul/Code/evs/build/programs/cleos/cleos", //cleos的位置和钱包的位置
    "node": "http://evo-chain.kkg222.com",   //node结点的http接口
    "wallet_passwd": "PW5KRu92ZLqjCG4L4wNNKkp48jPzRSiwo8fepYts6njKXo4LyJqC8", //钱包密码
    "funds_user": "deposit",  //提现转出的账户
    "contract_user": "deposit"   //合约账户，可以写一个合约，来限制用户的转入和转出。（例如只有填写指定memo的用户才能充值成功，充值的最小金额等。）
}
```

## 启动程序
`sudo restart.sh`

## 调用接口
程序提供http接口来提现，下面用是curl命令调用接口的示例
1. 插入memo
curl http://10.235.20.85:8082 -d '{"id":16,"method":"contract.insert_memo","params":[66]}'
说明：当用户要充值时，需要给用户一个memo值，币服务器会分析出memo，后台会根据memo找到对应的用户。memo要是一个数值
2. 提现操作
curl http://10.235.20.85:8082 -d '{"id":16,"method":"balance.withdraw","params":["eosio", "50.0000 TGC", 66, 10001]}'
说明： 从账户中转账50.0000个TGC到eosio账户，转账memo是"66 10001"，如是您业务转账不需要memo等额外信息，可以随便填写。
格外说明：
调用提现接口，一定要确保用户账户上有钱。
调用提现接口，一定要确保用户账户上有钱。
调用提现接口，一定要确保用户账户上有钱。
如果用户没有那么多钱，但我们的提现账户有这么多钱，那么这笔转账在链上是成功的，会转给用户，所以这个接口也返回成功；
但用户交易所账户没有这么多钱，所以币服务器调用撮合接口的时候会失败，导致用户扣不了钱。
3. 删除memo。
curl http://10.235.20.85:8082 -d '{"id":16,"method":"contract.erase_memo","params":[66]}'