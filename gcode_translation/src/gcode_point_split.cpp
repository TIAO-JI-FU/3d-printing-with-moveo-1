#include <ros/ros.h>
#include <sstream>
#include <fstream>
#include <time.h>
#include <math.h>
#include <qprogressdialog.h>
#include <QApplication>
#include <QString>
#include "std_msgs/Float64MultiArray.h"

int decimal_point(double &A){
  std::string change = std::to_string(A);
  bool point = 0;
  for(int i = 0;i<change.size();i++){
    if(change[i] == '.') point = 1;
    if(point == 1){
      if(change[i+3] == '0'){
        if(change[i+2] == '0'){
          if(change[i+1] == '0') return 0;
          return 1;
        }
        return 2;
      }
      return 3;
    }
  }
}

ros::WallTime start_, end_;
int main(int argc, char **argv){
  start_ = ros::WallTime::now();
  ros::init(argc, argv, "gcode_point_split");
  ros::NodeHandle node_handle("~");
  ros::AsyncSpinner spinner(1);
  spinner.start();

  ros::Publisher IK_pub = node_handle.advertise<std_msgs::Float64MultiArray>("start", 1000);
  //----------------------------
  //Setup
  //----------------------------

  std::string gcode_in, register_gcode_out;
  node_handle.param("gcode_in", gcode_in, std::string("/gcode_in"));
  node_handle.param("register_gcode_out", register_gcode_out, std::string("/register_gcode_out"));

  int all_lines = 0;
  std::string line;
  std::ifstream input_file(gcode_in);
  if(!input_file.is_open()) ROS_ERROR_STREAM("Can't open " <<gcode_in);
  while(input_file){
    std::getline(input_file, line);
    all_lines++;
  }
  input_file.close();
  input_file.open(gcode_in);

  // Write check_success
  std::string check_success;
  node_handle.param("check_success", check_success, std::string("/check_success"));
  std::ofstream output_file(check_success);
  if(!output_file.is_open()) ROS_ERROR_STREAM("Can't open " << check_success);
  output_file << "Error";
  output_file.close();

  ROS_INFO_STREAM("Write check success");
  output_file.open(register_gcode_out);
  if(!output_file.is_open()) ROS_ERROR_STREAM("Can't open " << register_gcode_out);
  
  QApplication app(argc, argv);
  QProgressDialog dialog("Connecting", "Cancel", 0, all_lines);
  dialog.setWindowTitle("Connecting");
  dialog.setWindowModality(Qt::WindowModal);
  dialog.show();
  dialog.setLabelText("Connecting to Gcode Translation");

  int now_line = 0;
  bool check_distance = 0;
  double x = 0;
  double y = 0;
  double z = 0;
  double E = 0;
  double pre_x = 0;
  double pre_y = 0;
  double pre_z = 0;
  double pre_E = 0;
  while(input_file){
    if(ros::ok()){
      std::getline(input_file, line);
      now_line++;
      if((!line.compare(0,2,"G0") || !line.compare(0,2,"G1"))){
        size_t colon_pos_X = line.find('X');
        if(colon_pos_X < 100) x = stod(line.substr(colon_pos_X+1));
        size_t colon_pos_Y = line.find('Y');
        if(colon_pos_Y < 100) y = stod(line.substr(colon_pos_Y+1));
        size_t colon_pos_Z = line.find('Z');
        if(colon_pos_Z < 100) z = stod(line.substr(colon_pos_Z+1));
        size_t colon_pos_E = line.find('E');
        if(colon_pos_E < 100 && (stod(line.substr(colon_pos_E+1)) != 0)) E = stod(line.substr(colon_pos_E+1));

        if(check_distance == 1){
          output_file << line[0] << line[1];
          double X_diff = (x - pre_x);
          double Y_diff = (y - pre_y);
          double E_diff = (E - pre_E);
          int cut_part = ceil(sqrt(pow(X_diff,2)+pow(Y_diff,2))/15);
          if(cut_part > 1){
            size_t colon_pos_F = line.find('F');
            if(colon_pos_F < 100 && stoi(line.substr(colon_pos_F+1)) != 0) output_file << " F" << stoi(line.substr(colon_pos_F+1));
            for(int i = 1; i < cut_part; i++){
              pre_x += X_diff/cut_part;
              pre_y += Y_diff/cut_part;
              pre_E += E_diff/cut_part;
              output_file << std::fixed << std::setprecision(decimal_point(pre_x)) << " X" << pre_x << std::defaultfloat;
              output_file << std::fixed << std::setprecision(decimal_point(pre_y)) << " Y" << pre_y << std::defaultfloat;
              if(colon_pos_E < 100 && stod(line.substr(colon_pos_E+1)) != 0) output_file << std::fixed << std::setprecision(5) << " E" << pre_E << std::defaultfloat;
              if(i == 1) output_file << " K0";
              output_file << std::endl;
              output_file << line[0] << line[1];
            }
            if(line.find('F') < 100){
              if(colon_pos_X < 100) output_file << std::fixed << std::setprecision(decimal_point(x)) << " X" << x << std::defaultfloat;
              if(colon_pos_Y < 100) output_file << std::fixed << std::setprecision(decimal_point(y)) << " Y" << y << std::defaultfloat;
              if(colon_pos_Z < 100) output_file << std::fixed << std::setprecision(decimal_point(z)) << " Z" << z << std::defaultfloat;
              if(colon_pos_E < 100 && (stod(line.substr(colon_pos_E+1)) != 0)) output_file << std::fixed << std::setprecision(5) << " E" << E << std::defaultfloat;
            }
            else{
              for(int j = 2;j < line.length(); j++){
                output_file << line[j];
              }
            }
          }
          else{
            for(int j = 2;j < line.length(); j++){
              output_file << line[j];
            }    
          }
          if(cut_part > 1) output_file << " K1";
          output_file << std::endl;
        }
        else output_file << line << std::endl;
        pre_x = x;
        pre_y = y;
        pre_E = E;
        check_distance = 1;
      }
      else output_file << line << std::endl;
      dialog.setValue(now_line);
      QCoreApplication::processEvents();
      if(dialog.wasCanceled()) ros::shutdown();
    }
    else return -1;
  }
  end_ = ros::WallTime::now();
  double execution_time = (end_ - start_).toNSec() * 1e-9;
  ROS_INFO_STREAM("Exectution time (s): " << execution_time);
  input_file.close();
  output_file.close();
  // Let inverse_kinematics start
  std_msgs::Float64MultiArray push;
  push.data.resize(1);
  push.data[0] = 1;
  IK_pub.publish(push);
  // Wait for all steps finish
  while(ros::ok()){
    ros::spinOnce();
  }
  return 0;
}