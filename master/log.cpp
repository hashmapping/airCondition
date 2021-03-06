#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTableView>
#include <QAbstractTableModel>


/*
table: logfile
room_id | check_in_time | check_out_time | user_id | cost |
*/

/*
 OnManualSubmit表明我们要提交修改才能使其生效。
 可以先将修改保存起来，当我们执行提交函数时，再去真正地修改数据库。
 当然，这个模型比前面的模型更高级，前面讲的所有操作，在这里都能执行。
*/

//实时显示数据表:logfile
void MainWindow::initializeLog()
{
    logModel = new QSqlTableModel(this);
    logModel->setTable("logfile");

    //表头重命名
    logModel->setHeaderData(logModel->fieldIndex("room_id"),Qt::Horizontal,"房间号");
    logModel->setHeaderData(logModel->fieldIndex("check_in_time"),Qt::Horizontal,"入住时间");
    logModel->setHeaderData(logModel->fieldIndex("check_out_time"),Qt::Horizontal,"退房时间");
    logModel->setHeaderData(logModel->fieldIndex("user_id"),Qt::Horizontal,"身份证号");
    logModel->setHeaderData(logModel->fieldIndex("cost"),Qt::Horizontal,"电费");

    logModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    //选取整个表的所有行
    logModel->select();
    //将表logModel绑定给tableview:tvlog
    ui->tvLog->setModel(logModel);
    //自适应填充窗口
    ui->tvLog->resizeColumnsToContents();
    ui->tvLog->horizontalHeader();
    for(int i = 0; i < ui->tvLog->horizontalHeader()->count(); i++){
        ui->tvLog->setColumnWidth(i, ui->tvLog->columnWidth(i)+80);
    }
    ui->tvLog->horizontalHeader()->setStretchLastSection(true);
}

//重置按钮:
/*
#点击重置按钮,弹出验证对话框,输入管理员帐号密码
#验证成功,则清空所有日志数据
#验证失败,弹出响应信息:验证失败,无法进行重置操作!
*/
void MainWindow::on_pbLogReset_clicked()
{

}

//刷新按钮:
/*
#点击刷新按钮
*/



