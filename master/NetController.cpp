#include "NetController.h"
#include "constant.h"
#include "datastruct.h"
#include "tcpclientsocket.h"
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QString>

//在什么情况下修改数据库表
/*
送风和停风请求:修改 room_state: wind,last_open_time,in_connect
定时发state(current temp)信息,修改 current_temp
*/

/*
#如何更新end_time?
#当下一个同房间的请求发来的时候,更新当前请求的end_time
*/

/*
当日报表:
房间号 | 开机次数 | 当日费用 | 
#如果当前时间是00:00,新建一条记录: daily_report
#
#t.currentMSecsSinceEpoch(): (current_time - Eopch_time)

*/

/*
#开机次数计算 : 收到每一次开机请求后,开机次数+1
#当日费用怎么算?
#daily_cost += sum( netInfo.cost )
#daily_cost += 昨日正在进行的送风和持续到明天的送风
*/



NetController::NetController(QObject *parent) :
    QTcpServer(parent)
{
    this->port = 6666;
    this->startListening();
    qDebug() << "start\n";
}
/*
void Server::checkAdminInfo(QString id, QString password)
{
    //管理员登陆成功
    emit sendAdminInfoToUi(isCorrect);
    if(isCorrect)
        this->startListening();
}
*/

//发送登陆请求回复
void NetController::sendReply(QTcpSocket* tsock,int reply,int workmode,int worktemp){
    QJsonObject rfl;
    rfl.insert("Type","ReplyForLogin");
    rfl.insert("Reply",reply);
    rfl.insert("WorkMode",workmode);
    rfl.insert("WorkTemperature",worktemp);
    qDebug()<<rfl<<endl;//------------------------------------------------------------------------debug
    send(tsock,rfl);
}
//发送能耗和费用信息
void NetController::sendEnergyAndCost(QTcpSocket* tsock,double energy,double cost)
{
    QJsonObject eac;
    eac.insert("Type","EnergyAndCost");
    eac.insert("Energy",energy);
    eac.insert("Cost",cost);
    send(tsock,eac);
}
//发送接收状态信息回复
void NetController::sendReplyForState(QTcpSocket* tsock,bool ack){
    QJsonObject rfs;
    rfs.insert("Type","ReplyForState");
    rfs.insert("Ack",ack);
    send(tsock,rfs);
}
//发送请求送风回复
void NetController::sendReplyForWindSupply(QTcpSocket* tsock,bool ack){
    QJsonObject rfws;
    rfws.insert("Type","ReplyForWindSupply");
    rfws.insert("Ack",ack);
    send(tsock,rfws);
}
//发送停止送风回复
void NetController::sendReplyForStopWindSupply(QTcpSocket* tsock,bool ack){
    QJsonObject rfs;
    rfs.insert("Type","ReplyForStopWindSupply");
    rfs.insert("Ack",ack);
    send(tsock,rfs);
}
//发送开机信号
void NetController::sendPowerOn(QTcpSocket* tsock)
{
    QJsonObject po;
    po.insert("Type","PowerOn");
    send(tsock,po);
}
//发送关机信号
void NetController::sendPowerOff(QTcpSocket* tsock){
    QJsonObject pof;
    pof.insert("Type","PowerOff");
    send(tsock,pof);
}

//服务器监视socket
void NetController::ReadMessage(int no,QJsonObject obj)
{
       qDebug() << "readmessage" << endl;
       qDebug() << obj <<endl;
      // qDebug()<<MasterState<<endl;
       processMessage(no,obj);
}

/*
----------------------------------------数据包处理模块------------------------------
#共收到五种包:
# 从机请求登录 | 从机请求注销 | 从机送风请求 | 从机停风请求 | 从机当前温度
#不同包需要处理的不同字段:
#从机请求登录: 更新<is_connect>信息
#从机请求注销: 更新<is_connect>信息；更新<current_wind>信息；更新<current_temp>信息
#维护数据表netinfoTable

#需要发送的包:
# 当前累计费用
------------------------------------------------------------------------------------
*/
void NetController::processMessage(int no, QJsonObject obj)
{
    //qDebug()<<"fuck!!!!!!!!!!!!!!!!!!!!!!!!!!<<endl";
                                             //--------------------------------------------debug
    QString type = obj.value("Type").toString();
    //从机请求登录
    if(type == "AskLogin"){
        judgeLogin(no,obj);//-------------------------------------------------ok
        insertTableNetinfo(obj,ASKLOGIN);
    }
    //收到送风请求
    else if(type == "AskWindSupply"){
        updateTableRoomStateTup(obj,SENDWIND);
        judgeWindSupply(no,obj);
        insertTableNetinfo(obj,SENDWIND);
         //sendReplyForWindSupply(true);
    }
    //收到停止送风请求
    else if(type == "StopWindSupply"){
        //handleStop();
        stopSlaveWind(no,obj);
        updateTableRoomStateTup(obj,STOPWIND);
        //sendReplyForStopWindSupply(true);
    }
    //维护从控表->当前温度信息
    else if(type == "State"){
        updateTableRoomStateTup(obj,UPDATETEMP);
        modifySlaveTemp(no,obj);
        insertTableNetinfo(obj,UPDATETEMP);
        //sendReplyForState(no,true);
    }
    //删除roomid_close的信息表
    else if(type == "AskLogout"){
        removeSlaveInfo(no,obj);
        logoutSuccess(no,obj);
        insertTableNetinfo(obj,ASKLOGOUT);
    }
    else if(type == "ReplyForEnergyAndCost" || type == "ReplyForPowerOn" || type == "ReplyForPoserOff"){
        return;
    }

}

//子模块：发送JSON
void NetController::send(QTcpSocket* tsock,QJsonObject json)
{
        QJsonDocument doc;
        doc.setObject(json);
        QByteArray byte = doc.toJson(QJsonDocument::Compact);
        QString str(byte);
        QByteArray blocks;
        QDataStream out(&blocks,QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_5);
        out << (quint16)0;
        out << str;
        out.device()->seek(0);
        out << (quint16)(blocks.size() - sizeof(quint16));
        tsock->write(blocks);
        //tcpList.removeFirst();
        qDebug() << json.value("Type") << endl;
}

void NetController::judgeWindSupply(int no,QJsonObject obj){
    int roomid = obj.value("Room").toInt();
    int askTemp = obj.value("Temperature").toInt();
    int askWindSpeed = obj.value("WindSpeed").toInt();
    int windType = obj.value("WindType").toInt();
    //送风请求后续操作
    TcpClientSocket* tcpClientSocket;
    int index;
    for(int i = 0; i < tcpClientSocketList.count(); i++){
        tcpClientSocket = tcpClientSocketList.at(i);
        if(tcpClientSocket->no == no){
            index = i;
            break;
        }
    }
    sendReplyForWindSupply(tcpClientSocket,true);
}

void NetController::removeSlaveInfo(int no,QJsonObject obj){
    int roomid = obj.value("Room").toInt();
    room_list.remove(roomid);

    //room_list[roomid].clear();
    //从机关机
}

void NetController::stopSlaveWind(int no,QJsonObject obj){
    int roomid = obj.value("Room").toInt();
    TcpClientSocket* tcpClientSocket;
    int index;
    for(int i = 0; i < tcpClientSocketList.count(); i++){
        tcpClientSocket = tcpClientSocketList.at(i);
        if(tcpClientSocket->no == no){
            index = i;
            break;
        }
    }
    //从机停风
    sendReplyForStopWindSupply(tcpClientSocket,true);
}

void NetController::modifySlaveTemp(int no,QJsonObject obj){
    int roomid = obj.value("Room").toInt();
    int current_temp = obj.value("Temperature").toInt();
    //从机的当前温度
    TcpClientSocket* tcpClientSocket;
    int index;
    for(int i = 0; i < tcpClientSocketList.count(); i++){
        tcpClientSocket = tcpClientSocketList.at(i);
        if(tcpClientSocket->no == no){
            index = i;
            break;
        }
    }
    sendReplyForState(tcpClientSocket,true);
}

void NetController::incomingConnection(qintptr socketDescriptor){
    TcpClientSocket *tcpClientSocket = new TcpClientSocket(this);
    tcpClientSocket->no = this->no;
    no++;
    connect(tcpClientSocket,SIGNAL(sendToServer(int,QJsonObject)),
            this,SLOT(ReadMessage(int,QJsonObject)));
    connect(tcpClientSocket,SIGNAL(disconnected(int)),
            this,SLOT(slotDisconnected(int)));
    tcpClientSocket->setSocketDescriptor(socketDescriptor);
    tcpClientSocket->setSocketOption(QAbstractSocket::LowDelayOption,1);
    tcpClientSocketList.append(tcpClientSocket);
    qDebug() << "222\n";
}

void NetController::slotDisconnected(int descriptor){
    for(int i=0;i<tcpClientSocketList.count();i++){
        TcpClientSocket *item = tcpClientSocketList.at(i);
        if(item->socketDescriptor() == descriptor){
//            int no = item->no;
            tcpClientSocketList.removeAt(i);
            return;
        }
    }
    return;
}

//监听
void NetController::startListening()
{
    no = 0;
    this->listen(QHostAddress::Any, port);
    this->billTimer = new QTimer();
    connect(billTimer, SIGNAL(timeout()), this, SLOT(sendBillToSlave()));

    this->billTimer->start(5000); //默认五秒发送一次账单信息
    /*
    this->refreshTimer = new QTimer();
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(sendStateQueryPackageToSlave()));
    this->refreshTimer->start((int)(refreshRate * 1000));
    emit sendUpdateInfoToUi(refreshRate, workMode, status);*/
}

void NetController::closeServer()
{
    billTimer->stop();
    delete billTimer;
    /*
    refreshTimer->stop();
    delete refreshTimer;
*/
    this->tcpClientSocketList.clear();
    this->close();
}


/*
-------------------------从机登录校验模块-----------------------------------------
#在room_state中查找由<room_id,user_id>唯一标识的tuple
#如果成功找到,则判断从机是否已建立连接(in_connect)
#与从机建立连接(in_connect=1)
#更新数据表room_state last_open_time=now
----------------------------------------------------------------------------------
*/
void NetController::judgeLogin(int no,QJsonObject obj)
{
    //qDebug()<<"fuck!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<endl; //------------------------------debug
    int id = obj.value("Room").toInt();
    QString user_id = obj.value("ID").toString();
    TcpClientSocket* tcpClientSocket;
    int index;
    for(int i = 0; i < tcpClientSocketList.count(); i++){
        tcpClientSocket = tcpClientSocketList.at(i);
        if(tcpClientSocket->no == no){
            index = i;
            break;
        }
     }
    //if(房间号与身份证号匹配,且 !in_connect )登录成功
    if(loginSuccess(id,user_id)){
        TcpClientSocket* broad_tcp = tcpClientSocket;//--------------------------new
        room_list.insert(id,broad_tcp);
        sendReply(tcpClientSocket,0,0,25);
    }else{
        sendReply(tcpClientSocket,1,0,25);
    }
}

bool NetController::loginSuccess(int room_id,QString user_id)
{
    QSqlQuery q;
    q.prepare("SELECT * FROM room_state WHERE room_id=:rid AND user_id=:uid");
    q.bindValue(":rid",room_id);
    q.bindValue(":uid",user_id);
    q.exec();
    if(q.next()){
        int in_connect = q.value(7).toInt();
        if(in_connect){
            return 0;
        }else{
            QDateTime t_text =QDateTime::currentDateTime();
            QString t = t_text.toString("yyyy-MM-dd hh:mm:ss ddd");

            q.prepare("UPDATE room_state SET in_connect=:connect_info,last_open_time=:time WHERE room_id=:rid");
            q.bindValue(":connect_info",1);
            q.bindValue(":time",t);
            q.bindValue(":rid",room_id);
            q.exec();
            return 1;
        }
    }else{
        return 0;
    }
}


/*
-------------------------从机注销校验模块-----------------------------------------
#在room_state中查找由<room_id,user_id>唯一标识的tuple
#如果成功找到,则判断从机是否已建立连接(in_connect)
#与从机断开连接(in_connect=0)
#更新数据表room_state: is_connect=0, current_wind=0, current_temp = -1;
----------------------------------------------------------------------------------
*/

void NetController::logoutSuccess(int no, QJsonObject obj)
{
    int room_id = obj.value("Room").toInt();
    QSqlQuery q;
    q.prepare("SELECT * FROM room_state WHERE room_id=:rid");
    q.bindValue(":rid",room_id);
    q.exec();
    if(q.next()){
        qDebug() << "22\n";
        int in_connect = q.value(7).toInt();
        q.prepare("UPDATE room_state SET in_connect=:connect_info,current_wind=:cwind,current_temp=:ctemp WHERE room_id=:rid");
        q.bindValue(":connect_info",0);
        q.bindValue(":rid",room_id);
        q.bindValue(":cwind",0);
        q.bindValue(":ctemp",-1);
        q.exec();
        return;
    }
    return;
}

/*
--------------------------温控请求表处理模块--------------------------------------
#1.更新表room_state
#2.向表netinfo插入新数据
#3.函数接收参数: <obj,obj_type>

----------------------------------------------------------------------------------
*/

void NetController::updateTableRoomStateTup(QJsonObject obj, int objType)
{

}

void NetController::insertTableNetinfo(QJsonObject obj,int objType)
{
    //AskOpenSlave
    int roomid = obj.value("Room").toInt();
    QString userid = obj.value("ID").toString();
    QDateTime start_time_text =QDateTime::currentDateTime();
    QString start_time = start_time_text.toString("yyyy-MM-dd hh:mm:ss ddd");

}


//---------------------------------------------------------------------------------readTableRoomState
void NetController::sendBillToSlave()
{
    for (QMap<int,TcpClientSocket*>::iterator it = room_list.begin();it != room_list.end();it++)
    {
        int roomid = it.key();
        //查找到roomid对应的energy和cost
        double energy = 0.0;
        double cost = 0.0;
        energy += 1.0;
        cost += 1.0;
        sendEnergyAndCost(it.value(),energy,cost);
    }
}
