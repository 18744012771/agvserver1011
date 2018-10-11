#include "mainwindow.h"
#include <QApplication>
#include "network/acceptor.h"
#include "network/session.h"
#include "network/sessionbuffer.h"
#include "network/tcpsession.h"
#include "network/tcpacceptor.h"
#include "network/sessionmanager.h"
#include "network/protocol.h"
#include <iostream>
#include "sql/cppsqlite3.h"
#include "user/msgprocess.h"
#include "util/common.h"
#include "agv/agv.h"
#include <memory>
#include "agv/rosagv.h"



//void testAGV()
//{
//    g_threadPool.enqueue([&] {
//        // test ros agv
//        //rosAgvPtr agv(new rosAgv(1,"robot_0","192.168.8.206",7070));
//        rosAgvPtr agv(new rosAgv(1, "robot_0", "192.168.8.211", 7070));

//        std::cout << "AGV init...." << std::endl;

//        agv->init();
//        chipmounter *chip = new chipmounter(1, "chipmounter", "10.63.39.190", 1000);
//        //chipmounter *chip = new chipmounter(1,"chipmounter","192.168.8.101",1024);
//        chip->init();
//        agv->setChipMounter(chip);

//        sleep(20);

//        chipinfo info;
//        while (chip != nullptr)
//        {
//            if (!chip->isConnected())
//            {
//                //chip->init();
//                //std::cout << "chipmounter disconnected, need reconnect...."<< std::endl;
//            }

//            if (chip->getAction(&info))
//            {
//                std::cout << "new task ...." << "action: " << info.action << "point: " << info.point << std::endl;
//                if (agv->isAGVInit())
//                {
//                    chip->deleteHeadAction();
//                    agv->startTask(info.point, info.action);
//                    //agv->startTask( "2510", "loading");

//                }
//            }
//            /*else
//        {
//            std::cout << "new task for test...." << "action: " <<info.action<< "point: " <<info.point<< std::endl;

//            agv->startTask( "2511", "unloading");
//            //agv->startTask( "", "");
//            break;
//        }*/

//            sleep(1);
//        }
//    });


//    std::cout << "testAGV end...." << std::endl;
//
//}






void quit(int sig)
{
    g_quit = true;
    _exit(0);
}


//int main(int argc, char *argv[])
//{

//    signal(SIGINT, quit);
//    std::cout << "start server ..." << std::endl;

//    //0.日志输出
//    initLog();

//    //1.打开数据库
//    try {
//        g_db.open(DB_File);
//    }
//    catch (CppSQLite3Exception &e) {
//        combined_logger->error("sqlite error {0}:{1};", e.errorCode(), e.errorMessage());
//        return -1;
//    }

//    //2.载入地图
//    if (!MapManager::getInstance()->load()) {
//        combined_logger->error("map manager load fail");
//        return -2;
//    }

//    //3.初始化车辆及其链接
//    if (!AgvManager::getInstance()->init()) {
//        combined_logger->error("AgvManager init fail");
//        return -3;
//    }

//    //4.初始化任务管理
//    if (!TaskManager::getInstance()->init()) {
//        combined_logger->error("TaskManager init fail");
//        return -4;
//    }

//    //5.用户管理
//    UserManager::getInstance()->init();

//    //6.初始化消息处理
//    if (!MsgProcess::getInstance()->init()) {
//        combined_logger->error("MsgProcess init fail");
//        return -5;
//    }

//    //7.初始化日志发布
//    UserLogManager::getInstance()->init();

//    // test ros agv
//    //rosAgvPtr agv(new rosAgv(1,"robot_0","127.0.0.1",7070));
//    //agv->init();

//    //8.初始化电梯
//    ElevatorManager::getInstance()->init();


//    //9.初始化任务生成
//    TaskMaker::getInstance()->init();


//    //    10.初始化tcp/ip 接口
//    //tcpip服务
//    auto aID = SessionManager::getInstance()->addTcpAccepter(9999);
//    SessionManager::getInstance()->openTcpAccepter(aID);

//    //websocket fuwu
//    aID = SessionManager::getInstance()->addWebSocketAccepter(9998);
//    SessionManager::getInstance()->openWebSocketAccepter(aID);
//#ifdef DY_TEST
//    //agv server
//    aID = SessionManager::getInstance()->addTcpAccepter(6789);
//    SessionManager::getInstance()->openTcpAccepter(aID);
//    AgvManager::getInstance()->setServerAccepterID(aID);
//#endif
//    combined_logger->info("server init OK!");

//    while (!g_quit) {
//        usleep(50000);
//    }

//    spdlog::drop_all();

//    return 0;
//}




















int main(int argc, char *argv[]){
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
//    //1.打开数据库
//    try {
//        g_db.open(DB_File);
//    }
//    catch (CppSQLite3Exception &e) {
//        //combined_logger->error("sqlite error {0}:{1};", e.errorCode(), e.errorMessage());
//        return -1;
//    }

//    Sql *g_sql = new Sql();
//    g_sql->createConnection();

//    g_sql->checkTables();
    //SessionBuffer b;
   // b.size();
   // SessionBuffer c(b);
    std::cout<<"wait finished!"<<std::endl;
    //delete g_sql;

    return a.exec();
}
