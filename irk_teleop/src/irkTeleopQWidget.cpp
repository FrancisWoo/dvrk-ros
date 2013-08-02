#include <ros/package.h>

#include "irk_teleop/irkTeleopQWidget.h"
#include "irk_kinematics/mtm_logic.h"
#include "irk_kinematics/psm_logic.h"

// set up joint state variables

irkTeleopQWidget::irkTeleopQWidget(const std::string &name, const double &period):
    QWidget(), counter_(0)
{
    // subscriber
    // NOTE: queue size is set to 1 to make sure data is fresh
    sub_mtm_pose_ = nh_.subscribe("/irk_mtm/cartesian_pose_current", 1,
                                    &irkTeleopQWidget::master_pose_cb, this);
    sub_psm_pose_ = nh_.subscribe("/irk_psm/cartesian_pose_current", 1,
                                   &irkTeleopQWidget::slave_pose_cb, this);

    // publisher
    pub_teleop_enable_ = nh_.advertise<std_msgs::Bool>("/irk_teleop/enable", 100);
    pub_mtm_control_mode_ = nh_.advertise<std_msgs::Int8>("/irk_mtm/control_mode", 100);
    pub_psm_control_mode_ = nh_.advertise<std_msgs::Int8>("/irk_psm/control_mode", 100);
    pub_enable_slider_ = nh_.advertise<sensor_msgs::JointState>("/irk_mtm/joint_state_publisher/enable_slider", 100);

    // pose display
    mtm_pose_qt_ = new vctQtWidgetFrame4x4DoubleRead;
    psm_pose_qt_ = new vctQtWidgetFrame4x4DoubleRead;
//    mtm_pose_qt_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//    psm_pose_qt_->resize(200, 200);

    QVBoxLayout *poseLayout = new QVBoxLayout;
    poseLayout->addWidget(mtm_pose_qt_);
    poseLayout->addWidget(psm_pose_qt_);

    // common console
    QGroupBox *consoleBox = new QGroupBox("Console");
    QPushButton *consoleHomeButton = new QPushButton(tr("Home"));
    QPushButton *consoleManualButton = new QPushButton(tr("Manual"));
    QPushButton *consoleTeleopTestButton = new QPushButton(tr("TeleopTest"));
    QPushButton *consoleTeleopButton = new QPushButton(tr("Teleop"));
    consoleHomeButton->setStyleSheet("font: bold; color: green;");
    consoleManualButton->setStyleSheet("font: bold; color: red;");
    consoleTeleopTestButton->setStyleSheet("font: bold; color: blue;");
    consoleTeleopButton->setStyleSheet("font: bold; color: brown;");
    consoleHomeButton->setCheckable(true);
    consoleManualButton->setCheckable(true);
    consoleTeleopTestButton->setCheckable(true);
    consoleTeleopButton->setCheckable(true);

    QButtonGroup *consoleButtonGroup = new QButtonGroup;
    consoleButtonGroup->setExclusive(true);
    consoleButtonGroup->addButton(consoleHomeButton);
    consoleButtonGroup->addButton(consoleManualButton);
    consoleButtonGroup->addButton(consoleTeleopTestButton);
    consoleButtonGroup->addButton(consoleTeleopButton);

    QHBoxLayout *consoleBoxLayout = new QHBoxLayout;
    consoleBoxLayout->addWidget(consoleHomeButton);
    consoleBoxLayout->addWidget(consoleManualButton);
    consoleBoxLayout->addWidget(consoleTeleopTestButton);
    consoleBoxLayout->addWidget(consoleTeleopButton);
    consoleBoxLayout->addStretch();
    consoleBox->setLayout(consoleBoxLayout);

    // mtm console
    QGroupBox *mtmBox = new QGroupBox("MTM");
    QVBoxLayout *mtmBoxLayout = new QVBoxLayout;
    mtmClutchButton = new QPushButton(tr("Clutch"));
    mtmClutchButton->setCheckable(true);
    mtmBoxLayout->addWidget(mtmClutchButton);

    mtmHeadButton = new QPushButton(tr("Head"));
    mtmHeadButton->setCheckable(true);
    mtmBoxLayout->addWidget(mtmHeadButton);

    mtmBoxLayout->addStretch();
    mtmBox->setLayout(mtmBoxLayout);

    // psm console
    QGroupBox *psmBox = new QGroupBox("PSM");
    QVBoxLayout *psmBoxLayout = new QVBoxLayout;
    psmMoveButton = new QPushButton(tr("Move Tool"));
    psmMoveButton->setCheckable(true);
    psmBoxLayout->addWidget(psmMoveButton);
    psmBoxLayout->addStretch();
    psmBox->setLayout(psmBoxLayout);

    // rightLayout
    QGridLayout *rightLayout = new QGridLayout;
    rightLayout->addWidget(consoleBox, 0, 0, 1, 2);
    rightLayout->addWidget(mtmBox, 1, 0);
    rightLayout->addWidget(psmBox, 1, 1);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(poseLayout);
    mainLayout->addLayout(rightLayout);

    this->setLayout(mainLayout);
    this->resize(sizeHint());
    this->setWindowTitle("Teleopo GUI");

    // set stylesheet
    std::string filename = ros::package::getPath("irk_teleop");
    filename.append("/src/default.css");
    QFile defaultStyleFile(filename.c_str());
    defaultStyleFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(defaultStyleFile.readAll());
    this->setStyleSheet(styleSheet);

    // now connect everything
    connect(consoleHomeButton, SIGNAL(clicked()), this, SLOT(slot_homeButton_pressed()));
    connect(consoleManualButton, SIGNAL(clicked()), this, SLOT(slot_manualButton_pressed()));
    connect(consoleTeleopTestButton, SIGNAL(clicked()), this, SLOT(slot_teleopTestButton_pressed()));
    connect(consoleTeleopButton, SIGNAL(clicked()), this, SLOT(slot_teleopButton_pressed()));

    connect(mtmHeadButton, SIGNAL(clicked(bool)), this, SLOT(slot_headButton_pressed(bool)));
    connect(mtmClutchButton, SIGNAL(clicked(bool)), this, SLOT(slot_clutchButton_pressed(bool)));
    connect(psmMoveButton, SIGNAL(clicked(bool)), this, SLOT(slot_moveToolButton_pressed(bool)));

    // show widget & start timer
    startTimer(period);  // 50 ms
}


void irkTeleopQWidget::timerEvent(QTimerEvent *)
{
    // check ros::ok()
    if (!ros::ok()) {
        QApplication::quit();
    }

    // refresh data
    ros::spinOnce();

    mtm_pose_qt_->SetValue(mtm_pose_cur_);
    psm_pose_qt_->SetValue(psm_pose_cur_);

    // check teleop enable status
    std_msgs::Bool msg_teleop_enable;
    if (mtmHeadButton->isChecked() && !mtmClutchButton->isChecked() && !psmMoveButton->isChecked() )
    {
//        std::cerr << "YEAH ---teleop: " << counter_ << std::endl;
        msg_teleop_enable.data = true;
        pub_teleop_enable_.publish(msg_teleop_enable);
    } else {
        msg_teleop_enable.data = false;
        pub_teleop_enable_.publish(msg_teleop_enable);
    }

    counter_++;
}


void irkTeleopQWidget::master_pose_cb(const geometry_msgs::PoseConstPtr &msg)
{
    mtsROSToCISST((*msg), mtm_pose_cur_);
}

void irkTeleopQWidget::slave_pose_cb(const geometry_msgs::PoseConstPtr &msg)
{
    mtsROSToCISST((*msg), psm_pose_cur_);
}


// -------- slot -----------
void irkTeleopQWidget::slot_homeButton_pressed(void)
{
    msg_mtm_mode_.data = MTM::MODE_RESET;
    msg_psm_mode_.data = PSM::MODE_RESET;
    pub_mtm_control_mode_.publish(msg_mtm_mode_);
    pub_psm_control_mode_.publish(msg_psm_mode_);
}

void irkTeleopQWidget::slot_manualButton_pressed(void)
{
    msg_mtm_mode_.data = MTM::MODE_MANUAL;
    msg_psm_mode_.data = PSM::MODE_MANUAL;
    pub_mtm_control_mode_.publish(msg_mtm_mode_);
    pub_psm_control_mode_.publish(msg_psm_mode_);
}

void irkTeleopQWidget::slot_teleopTestButton_pressed(void)
{
    msg_mtm_mode_.data = MTM::MODE_TELEOP;
    msg_psm_mode_.data = PSM::MODE_TELEOP;
    pub_mtm_control_mode_.publish(msg_mtm_mode_);
    pub_psm_control_mode_.publish(msg_psm_mode_);
}

void irkTeleopQWidget::slot_teleopButton_pressed(void)
{
    msg_mtm_mode_.data = MTM::MODE_TELEOP;
    msg_psm_mode_.data = PSM::MODE_HOLD;
    pub_mtm_control_mode_.publish(msg_mtm_mode_);
    pub_psm_control_mode_.publish(msg_psm_mode_);
}

void irkTeleopQWidget::slot_clutchButton_pressed(bool state)
{
    if (state) {
        // clutch MOVE
        ROS_ERROR_STREAM("MOVE IT");
    } else {
        // no MOVE
        ROS_ERROR_STREAM("HOLD IT!");
    }
}

void irkTeleopQWidget::slot_headButton_pressed(bool state)
{
    sensor_msgs::JointState msg_js;
    msg_js.name.clear();
    msg_js.name.push_back("right_outer_yaw_joint");
    msg_js.name.push_back("right_shoulder_pitch_joint");
    msg_js.name.push_back("right_elbow_pitch_joint");
    msg_js.name.push_back("right_wrist_platform_joint");
    msg_js.name.push_back("right_wrist_pitch_joint");
    msg_js.name.push_back("right_wrist_yaw_joint");
    msg_js.name.push_back("right_wrist_roll_joint");

    msg_js.position.resize(7);
    msg_js.position[0] = 1;
    msg_js.position[1] = 1;
    msg_js.position[2] = 1;
    msg_js.position[3] = 1;
    msg_js.position[4] = 1;
    msg_js.position[5] = 1;
    msg_js.position[6] = 1;

    if (state) {
        pub_enable_slider_.publish(msg_js);
    } else {
        msg_js.position[3] = -1;
        msg_js.position[4] = -1;
        msg_js.position[5] = -1;
        msg_js.position[6] = -1;
        pub_enable_slider_.publish(msg_js);
    }
}


void irkTeleopQWidget::slot_moveToolButton_pressed(bool state)
{
    if (state) {
        msg_psm_mode_.data = PSM::MODE_MANUAL;  // MANUAL
    } else {
        msg_psm_mode_.data = PSM::MODE_HOLD;  // HOLD
    }
    pub_psm_control_mode_.publish(msg_psm_mode_);
}

















