#include <iostream>
#include <vector>
#include <cmath>
#include <sstream>
#include <string>
#include <limits>
#include <chrono>
#include <thread>


#include "util/UtilDriver.h"
#include "util/UtilMath.h"
#include "util/GetSignType.h"
#include "bizier.h"


#include "SimOneSensorAPI.h"
#include "SimOneV2XAPI.h"
#include "SimOneServiceAPI.h"
#include "SimOneHDMapAPI.h"
#include "SimOnePNCAPI.h"
#include "SSD/SimPoint3D.h"
#include "SSD/SimString.h"
#include "public/common/MLaneInfo.h"
#include "public/common/MLaneId.h"
#include "utilTargetLane.h"
#include "pid.hpp"
#include "prediction.h"
#include "SSD/SimPoint2D.h"
#include "SimOneEvaluationAPI.h"



//const char* MainVehicleID = "0";

struct obstaclestruct {
  SSD::SimPoint3D pt;
  SSD::SimString ownerLaneId;
  ESimOne_Obstacle_Type type;
  double speed = 100;
  int id{};
};

double CalculateDistance(SSD::SimPoint3D &pt1, SSD::SimPoint3D &pt2) {
  double dis;
  dis = (pt1.x - pt2.x) * (pt1.x - pt2.x) + (pt1.y - pt2.y) * (pt1.y - pt2.y) + (pt1.z - pt2.z) * (pt1.z - pt2.z);
  dis = sqrtf(dis);
  return dis;
}


double CalculateSpeed(SimOne_Data_Gps *gpsdata) {
  double v = 0;
  v = sqrtf(pow(gpsdata->velX, 2) + pow(gpsdata->velY, 2)) * 3.6;
  return v;
}


//֥ʿ�������������Ϣ
SSD::SimString GetNearMostLane(const SSD::SimPoint3D &pos) {
  SSD::SimString laneId;
  double s, t, s_toCenterLine, t_toCenterLine;
  if (!SimOneAPI::GetNearMostLane(pos, laneId, s, t, s_toCenterLine, t_toCenterLine)) {
    std::cout << "Error: lane is not found." << std::endl;
  }
  return laneId;
}
//֥ʿ����s��t

double GetS(const SSD::SimPoint3D &pos, SSD::SimString laneId) {
  double s, t;
  if (!SimOneAPI::GetLaneST(laneId, pos, s, t)) {
    //std::cout << "Error: lane is not found." << std::endl;
  }
  return s;
}

double GetT(const SSD::SimPoint3D &pos, SSD::SimString laneId) {
  double s, t;
  if (!SimOneAPI::GetLaneST(laneId, pos, s, t)) {
    //std::cout << "Error: lane is not found." << std::endl;
  }
  return t;
}


//��������ʼ�㵽��ֹ��ĵ�·id�б�
SSD::SimVector<long> GetNavigateRoadIdList(const SSD::SimPoint3D &startPt, const SSD::SimPoint3D &endPt) {
  SSD::SimVector<long> naviRoadIdList;
  SSD::SimPoint3DVector ptList;
  ptList.push_back(startPt);
  ptList.push_back(endPt);
  SSD::SimVector<int> indexOfValidPoints;
  SimOneAPI::Navigate(ptList, indexOfValidPoints, naviRoadIdList);//��ȡ�滮·����;����·��ID�б�
  return std::move(naviRoadIdList);
}


//��ȡ��ǰlane����һ��lane


bool GetValidSuccessor(const HDMapStandalone::MLaneId &laneId, const long &currentRoadId, const long &nextRoadId,
                       HDMapStandalone::MLaneId &successor) {
  if (nextRoadId == -1)                                                //��һ��roadid
  {
    return false;
  }
  HDMapStandalone::MLaneLink laneLink;                                //����string��ʽ�ĳ����б�
  bool valid = SimOneAPI::GetLaneLink(laneId.ToString(), laneLink);    //��ȡ�ٽ�������Ϣ
  assert(valid);
  if (laneLink.successorLaneNameList.empty()) {

    return false;
  }

  for (auto &successorLane: laneLink.successorLaneNameList) {
    HDMapStandalone::MLaneId successorId(successorLane);            //��string���ݱ������
    if (successorId.roadId != currentRoadId)                        //��һ������road�����ڵ�ǰroad
    {
      if (successorId.roadId == nextRoadId)                        //������ȥ��nextroad����������
      {
        successor = successorId;
        return true;                                            //API��ȡ����һ������Ϣ�������nextRoadId��ͬ
      }
    } else                                                            //�������һ��section�ͷ���true�����û�оͷ���false
    {
      HDMapStandalone::MLaneId successorOfSuccessor;
      if (successorId.sectionIndex == laneId.sectionIndex + 1)    //����ݹ麯����֪����û�г�����
      {
        successor = successorId;                                //�������successorLane��ֵ��ͬ
        return true;
      }
    }
  }
  return false;
}


//��ȡ��ǰ�����������б�ӵ�targetpath��


void AddSamples(const HDMapStandalone::MLaneId &laneId, SSD::SimPoint3DVector &path) {
  HDMapStandalone::MLaneInfo laneInfo;                                                        //laneinfo��һ���㼯��������������
  if (SimOneAPI::GetLaneSample(laneId.ToString(), laneInfo)) {
    path.reserve(path.size() + laneInfo.centerLine.size());                                    //��path��������ռ�
    for (auto &pt: laneInfo.centerLine) {
      path.push_back(pt);
    }
    //std::cout << " findpoint: " << path.back().x << ", " << path.back().y << std::endl;
    //std::cout << " laneid: " << laneId.roadId << ", " << laneId.sectionIndex << ", " << laneId.laneId << std::endl;
  }
}


//����path

SSD::SimPoint3DVector GetReferencePath(const SSD::SimPoint3D &startPt, const SSD::SimVector<long> &naviRoadIdList) {
  SSD::SimPoint3DVector path;
  SSD::SimString laneName;
  double s = 0, t = 0, s_toCenterLine, t_toCenterLine;
  HDMapStandalone::MLaneInfo info;

  if (!SimOneAPI::GetNearMostLane(startPt, laneName, s, t, s_toCenterLine, t_toCenterLine)) {
    return path;
  }
  SSD::SimString laneId;
  HDMapStandalone::MLaneId currentLaneId(laneId);
  HDMapStandalone::MLaneId nextLaneId;

  AddSamples(currentLaneId, path);

  for (int index = 0; index < naviRoadIdList.size(); ++index) {
    long roadId = naviRoadIdList[index];
    long nextRoadId = (index + 1 < naviRoadIdList.size()) ? naviRoadIdList[index + 1] : -1;

    if (GetValidSuccessor(currentLaneId, roadId, nextRoadId, nextLaneId)) {
      AddSamples(nextLaneId, path);
      currentLaneId = nextLaneId;
    } else {
      break;
    }
  }

  return std::move(path);
}

//ʶ�����ٱ�ʾ���Լ�����r������speed




//��ȡ�ϰ����б�
std::vector<obstaclestruct> GetObstacleList() {
  std::vector<obstaclestruct> allObstacle;
  /*SSD::SimPoint3D pt;
  SSD::SimString ownerLaneId;*/
  std::unique_ptr<SimOne_Data_Obstacle> pSimOne_Data_Obstacle = std::make_unique<SimOne_Data_Obstacle>();
  if (SimOneAPI::GetGroundTruth(0, pSimOne_Data_Obstacle.get())) {
    //cout << SimOneSM::GetGroundTruth(0, pSimOne_Data_Obstacle.get()) << " size: " << pSimOne_Data_Obstacle->obstacleSize << endl;
    //std::cout <<"obstacleSize+++++++++++++++++++++"<< pSimOne_Data_Obstacle->obstacleSize << std::endl;
    for (int i = 0; i < pSimOne_Data_Obstacle->obstacleSize; i++) {
      float posX = pSimOne_Data_Obstacle->obstacle[i].posX;
      float posY = pSimOne_Data_Obstacle->obstacle[i].posY;
      float posZ = pSimOne_Data_Obstacle->obstacle[i].posZ;
      SSD::SimPoint3D pos(posX, posY, posZ);
      SSD::SimString laneId = GetNearMostLane(pos);
      ESimOne_Obstacle_Type type = pSimOne_Data_Obstacle->obstacle[i].type;
      int id = pSimOne_Data_Obstacle->obstacle[i].id;
      double speed = UtilMath::calculateSpeed(pSimOne_Data_Obstacle->obstacle[i].velX,
                                              pSimOne_Data_Obstacle->obstacle[i].velY,
                                              pSimOne_Data_Obstacle->obstacle[i].velZ);
      allObstacle.push_back(obstaclestruct{SSD::SimPoint3D(posX, posY, posZ), laneId, type, speed, id});
    }
  }
  return allObstacle;//�������vector����;
}

//������һ�������ͬ�����ϰ���(����������ж���Ҫ��ӵ�·���紦���ж�)
int GetNearObstacleIndex(const SSD::SimPoint3D &mainVehiclepos, const double &mainVehiclespeed,
                         const vector<obstaclestruct> &Obstaclelist, const SSD::SimString &currentlaneId) {
  int obstacleClosestIndex = -1;
  SSD::SimString mainVehicleLane = GetNearMostLane(mainVehiclepos);
  double minDist = std::numeric_limits<double>::max();//��ʼ����С����
  SSD::SimPoint3D vehiclePos3D(mainVehiclepos.x, mainVehiclepos.y, mainVehiclepos.z);
  HDMapStandalone::MLaneLink lanelink;
  SimOneAPI::GetLaneLink(currentlaneId, lanelink);
  for (unsigned int i = 0; i < Obstaclelist.size(); i++) {
    //�����ϰ���

    auto &obstacle = Obstaclelist[i];
    double distanceSign = UtilMath::distance(vehiclePos3D,
                                             SSD::SimPoint3D(obstacle.pt.x, obstacle.pt.y, obstacle.pt.z));
    if (GetS(mainVehiclepos, mainVehicleLane) > GetS(obstacle.pt, mainVehicleLane))//���������ϰ�������������ĺ󷽣����ж��ٶȣ����жϾ���
    {
      if (obstacle.speed > mainVehiclespeed + 2 && distanceSign < minDist &&
          (obstacle.ownerLaneId == currentlaneId ||
           count(lanelink.predecessorLaneNameList.begin(), lanelink.predecessorLaneNameList.end(),
                 obstacle.ownerLaneId))) {
        minDist = distanceSign;
        obstacleClosestIndex = i;
      }
    } else//���������ϰ������������ǰ����ֱ���жϾ���
    {
      if (distanceSign < minDist && obstacle.ownerLaneId != lanelink.leftNeighborLaneName &&
          obstacle.ownerLaneId != lanelink.rightNeighborLaneName) {
        //cout << "i am in " << endl;
        minDist = distanceSign;
        obstacleClosestIndex = i;
      }
    }
  }
  return obstacleClosestIndex;
}

//������ҳ�����ȷ�������������һ������������㼯������true or false��
bool GetdetectingZone(const SSD::SimString &currentlaneId, SSD::SimString &changetolanename,
                      const SSD::SimPoint3D &mainVehiclepos, const obstaclestruct &Obstacle,
                      SSD::SimPoint3DVector &detectingZone, SSD::SimString &turn) {
  SSD::SimString turntolane;
  HDMapStandalone::MLaneLink lanelink;
  HDMapStandalone::MLaneType leftlanetype, rightlanetype;
  SimOneAPI::GetLaneLink(currentlaneId, lanelink);//��õ�ǰ����������Ϣ
  HDMapStandalone::MRoadMark left, right;

  SimOneAPI::GetRoadMark(mainVehiclepos, currentlaneId, left, right);
  SimOneAPI::GetLaneType(lanelink.leftNeighborLaneName, leftlanetype);
  SimOneAPI::GetLaneType(lanelink.rightNeighborLaneName, rightlanetype);
  if (lanelink.leftNeighborLaneName.Empty() == 0 && leftlanetype == HDMapStandalone::MLaneType::driving &&
      left.type == HDMapStandalone::ERoadMarkType::broken) {
    turntolane = lanelink.leftNeighborLaneName;
    turn = "left";
  } else if (lanelink.rightNeighborLaneName.Empty() == 0 && rightlanetype == HDMapStandalone::MLaneType::driving &&
             right.type == HDMapStandalone::ERoadMarkType::broken) {
    turntolane = lanelink.rightNeighborLaneName;
    turn = "right";
  } else {
    return false;
  }
  changetolanename = turntolane;
  SSD::SimPoint3D changeToPoint, changeBackPoint;//�����׼��ͱ任��
  SSD::SimPoint3D dir, dir2;
  double s, t, s_front, s_back, t_left, t_right;
  double distance = UtilMath::distance(mainVehiclepos, Obstacle.pt); //�������ϰ���ľ���

  SimOneAPI::GetLaneMiddlePoint(Obstacle.pt, turntolane, changeToPoint,
                                dir);//����ϰ����������೵����ͶӰ��API::GetLaneST(turntolane, changeToPoint, s, t);//���ͶӰ���st����
  t_left = t - 1.7;
  t_right = t + 1.7;
  s_front = s + 10;
  s_back = s - distance - 7;

  //�����������ĸ����st����ת����xyz���꣬������������㼯
  SimOneAPI::GetInertialFromLaneST(turntolane, s_front, t_right, changeBackPoint, dir2);
  detectingZone.push_back(changeBackPoint);
  SimOneAPI::GetInertialFromLaneST(turntolane, s_front, t_left, changeBackPoint, dir2);
  detectingZone.push_back(changeBackPoint);
  SimOneAPI::GetInertialFromLaneST(turntolane, s_back, t_left, changeBackPoint, dir2);
  detectingZone.push_back(changeBackPoint);
  SimOneAPI::GetInertialFromLaneST(turntolane, s_back, t_right, changeBackPoint, dir2);
  detectingZone.push_back(changeBackPoint);

  return true;
}

//��������Ǽ�����������Ƿ�������
bool IsChangeLaneOccupied(const vector<obstaclestruct> &Obstaclelist, SSD::SimPoint3DVector &detectingZone) {
  for (auto &obstacle: Obstaclelist) {
    if (IsOccupied(obstacle.pt, detectingZone)) {
      return true;
    }
  }
  return false;
}

//s�������е����ĵ�
void GetLaneSampleFromS(const SSD::SimString &laneId, const double &s, SSD::SimPoint3DVector &targetPath) {
  HDMapStandalone::MLaneInfo info;
  if (SimOneAPI::GetLaneSample(laneId, info)) {
    double accumulated = 0.0;
    int startIndex = -1;
    for (unsigned int i = 0; i < info.centerLine.size() - 1; i++) {
      auto &pt = info.centerLine[i];
      auto &ptNext = info.centerLine[i + 1];
      double d = UtilMath::distance(pt, ptNext);
      accumulated += d;
      if (accumulated >= s) {
        startIndex = i + 1;
        break;
      }
    }
    for (unsigned int i = startIndex; i < info.centerLine.size(); i++) {
      SSD::SimPoint3D item = info.centerLine[i];
      targetPath.push_back(info.centerLine[i]);
    }
  }
}

//s���󵥸����ĵ�
bool GetSinglePointFromS(const SSD::SimString &laneId, const double &s, SSD::SimPoint3D &point) {
  HDMapStandalone::MLaneInfo info;
  if (SimOneAPI::GetLaneSample(laneId, info)) {
    double accumulated = 0.0;
    int startIndex = -1;
    for (unsigned int i = 0; i < info.centerLine.size() - 1; i++) {
      auto &pt = info.centerLine[i];
      auto &ptNext = info.centerLine[i + 1];
      double d = UtilMath::distance(pt, ptNext);
      accumulated += d;
      if (accumulated >= s) {
        startIndex = i + 1;
        break;
      }
    }
    for (unsigned int i = startIndex; i < info.centerLine.size(); i++) {
      SSD::SimPoint3D item = info.centerLine[i];
      point = item;
      return true;
    }
  }
  return false;
}

//����켣
SSD::SimPoint3DVector ChangeLanePathWithPoint(const SSD::SimPoint3D &from, const SSD::SimPoint3D &to) {
  SSD::SimPoint3DVector path;
  double s, t, s_c, t_c;
  SSD::SimPoint3D PointNextFrom, PointNextTo, dir;
  SSD::SimString lanefrom, laneto;
  SimOneAPI::GetNearMostLane(from, lanefrom, s, t, s_c, t_c);
  SimOneAPI::GetInertialFromLaneST(lanefrom, s + 1, t, PointNextFrom, dir);
  SimOneAPI::GetNearMostLane(to, laneto, s, t, s_c, t_c);
  SimOneAPI::GetInertialFromLaneST(laneto, s + 1, t, PointNextTo, dir);
  ChangeLanePath(int(UtilMath::distance(from, to)), from, PointNextFrom, to, PointNextTo, path);
  return path;
}


//�������������������һ�����ڱ����path�㼯
void GetChangeLanePath(const double &vel, const obstaclestruct &Obstacle, const SSD::SimPoint3D &mainVehiclepos,
                       const SSD::SimString &changetolaneName, SSD::SimPoint3DVector &targetpath) {
  targetpath.clear();//��һ����������ԭ�ȵ�·��
  SSD::SimString laneNow;
  SSD::SimPoint3D changeToPoint, changeToNextPoint, FromNextPoint;
  SSD::SimPoint3D dir;
  double fs, ft, fcs, fct, tos, tot;
  if (SimOneAPI::GetLaneMiddlePoint(Obstacle.pt, changetolaneName, changeToPoint,
                                    dir))//��ȡ�����ͶӰ��ָ�������������ϵĵ�����߷��򣨼��õ������ĵ㣩
  {
    //std::cout << "changeToPoint x:" << changeToPoint.x << "changeToPoint y:" << changeToPoint.y << "z:" << changeToPoint.z << std::endl;
  }

  SimOneAPI::GetLaneST(changetolaneName, changeToPoint, tos, tot);
  SimOneAPI::GetInertialFromLaneST(changetolaneName, tos + vel * 4, tot, changeToPoint, dir);
  targetpath = ChangeLanePathWithPoint(mainVehiclepos, changeToPoint);
}

//��i�������������Ƿ���lane�����ߵĵ㼯
SSD::SimPoint3DVector GetLaneSample(const SSD::SimString &laneId) {
  SSD::SimPoint3DVector targetPath;
  HDMapStandalone::MLaneInfo info;
  /*
      SSD::SimString laneName;
      SSD::SimPoint3DVector leftBoundary;
      SSD::SimPoint3DVector rightBoundary;
      SSD::SimPoint3DVector centerLine;
  */
  if (SimOneAPI::GetLaneSample(laneId, info)) {
    for (auto &pt: info.centerLine) {
      targetPath.push_back(SSD::SimPoint3D(pt.x, pt.y, pt.z));
    }
  }
  return targetPath;
}

//��������������ǵõ�һ�������ƿ��ϰ����targetpath�㼯
bool DetectObstacle(const SSD::SimPoint3D &vehiclePos, const std::vector<obstaclestruct> &allObstacles,
                    const HDMapStandalone::MLaneType &laneType,
                    const SSD::SimString &laneId, const SSD::SimString &leftNeighborLaneName,
                    SSD::SimPoint3DVector &targetPath, int &warningObstacleIndex, const double mainVehiclespeed) {
  warningObstacleIndex = -1;

  // ����ϰ����б�Ϊ�ջ򳵵����Ͳ�����ʻ�������򷵻�false
  if (allObstacles.empty() || laneType != HDMapStandalone::MLaneType::driving) {
    return false;
  }

  const double kDistanceSafety = 3 * mainVehiclespeed; // ��ȫ����Ϊ�����ٶȵ�3��

  // ��ȡ�����ڳ����е�ST����
  double vehs, veht;
  SimOneAPI::GetLaneST(laneId, vehiclePos, vehs, veht);

  // ����������ϰ���
  double minDist = std::numeric_limits<double>::max();
  int obstacleClosestIndex = -1;
  SSD::SimPoint2D vehiclePos2D(vehiclePos.x, vehiclePos.y);

  for (unsigned int i = 0; i < allObstacles.size(); i++) {
    auto &obstacle = allObstacles[i];

    // ���㳵��λ�����ϰ���λ�õ�ŷ�Ͼ���
    double distanceSign = UtilMath::distance(vehiclePos2D, SSD::SimPoint2D(obstacle.pt.x, obstacle.pt.y));

    // ��ȡ�ϰ����������������ϵ�ST����
    double s, t;
    SimOneAPI::GetLaneST(obstacle.ownerLaneId, SSD::SimPoint3D(obstacle.pt.x, obstacle.pt.y, obstacle.pt.z), s, t);

    // ����ϰ����Ƿ��ڳ���ǰ���Ҿ������
    if (distanceSign < minDist && obstacle.ownerLaneId == laneId && vehs < s) {
      minDist = distanceSign;
      obstacleClosestIndex = i;
    }
  }

  // ����ҵ�������ϰ���
  if (obstacleClosestIndex != -1) {
    auto &obstacleClosest = allObstacles[obstacleClosestIndex];

    // ���㳵��λ��������ϰ���λ�õ�ŷ�Ͼ���
    double distanceSign = UtilMath::distance(vehiclePos2D,
                                             SSD::SimPoint2D(obstacleClosest.pt.x, obstacleClosest.pt.y));

    // ������ϰ���ľ���С�ڵ��ڰ�ȫ����
    if (distanceSign <= kDistanceSafety) {
      warningObstacleIndex = obstacleClosestIndex;
      targetPath.clear();  // ���Ŀ��·��

      SSD::SimPoint3D changeToPoint;
      SSD::SimPoint3D dir;

      // ��ȡ�ϰ�����Ŀ�공���������ϵ�ͶӰ������߷���
      if (SimOneAPI::GetLaneMiddlePoint(obstacleClosest.pt, leftNeighborLaneName, changeToPoint, dir)) {
        std::cout << "changeToPoint x:" << changeToPoint.x << "changeToPoint y:" << changeToPoint.y << "z:"
                  << changeToPoint.z << std::endl;
      }

      targetPath.push_back(vehiclePos);
      SSD::SimVector<SSD::SimPoint3D> changeLanePath = ChangeLanePathWithPoint(vehiclePos, changeToPoint);

      // ��ƽ��������·������Ŀ��·��
      for (auto &knot: changeLanePath) {
        targetPath.push_back(knot);
      }

      targetPath.push_back(changeToPoint);

      return true; // ����true��ʾ��⵽�ϰ��ﲢ������Ŀ��·��
    }
  }

  return false; // ������ϰ���ľ�����ڰ�ȫ���룬����false
}

//�������ϰ���

bool PassedObstacle(const SSD::SimPoint3D &vehiclePos, const obstaclestruct &obstacle, const SSD::SimString &laneId) {
  double s, t;
  bool found = SimOneAPI::GetLaneST(laneId, obstacle.pt, s, t);//�õ��ϰ����ڳ����ϵ�st����
  assert(found);
  double s_vehicle, t_vehicle;
  found = SimOneAPI::GetLaneST(laneId, vehiclePos, s_vehicle, t_vehicle);//�õ������ڳ����ϵ�st����
  assert(found);
  return s_vehicle > s;//��������Ƿ��Ѿ�ͨ���ϰ���
}


//��ͨ�źŵ�ʶ��

//���ݵ�ǰʮ�ֻ�T��·�ڵ�roadid����ȡ��Ӧ���źŵ�

HDMapStandalone::MSignal GetTargetLightNow(const HDMapStandalone::MLaneId &id,
                                           const SSD::SimString &laneId,
                                           const SSD::SimVector<long> &roadIdList,
                                           bool &getLight) {
  HDMapStandalone::MSignal light;

  //��ȡ��ͼ���źŵ��б�
  SSD::SimVector<HDMapStandalone::MSignal> lightList;
  SimOneAPI::GetTrafficLightList(lightList);

  for (const auto &item: lightList) {  // ��ͨ���б����ÿһ����
    int num = 0;
    for (const auto &ptValidities: item.validities) {  // ��ͨ�ƶ�Ӧ��ÿһ����·��Ϣ
      auto it = std::find(roadIdList.begin(), roadIdList.end(), ptValidities.roadId);
      if (it != roadIdList.end()) {  // ������ܵ�··�����ҵ�light��һ��validities
        ++num;
        if (num >= 3) {
          break;  // ����Ѿ�ƥ�䵽�㹻�����ĵ�·������ǰ����ѭ��
        }
      }
      if (ptValidities.roadId == id.roadId) {  // ɸѡ����ǰ·�ڵĺ��̵�
        ++num;
        if (num >= 3) {
          break;  // ����Ѿ�ƥ�䵽�㹻�����ĵ�·������ǰ����ѭ��
        }
      }
    }
    if (num >= 3) {  // �����Ӧ������roadid����֤�����źŵ���Ҫ�ҵ��źŵ�
      light = item;
      getLight = true;
      break;
    }
  }
  return light;  // ��õ�ǰ·�ڶ�Ӧ�Ľ�ͨ��
}

//���ص�bool���������ж��Ƿ����ͨ�У�light.id = lightId��
bool IsGreenLight(const long &lightId, const SSD::SimString &laneId, const HDMapStandalone::MSignal &light, double speed,double distance) {
  SimOne_Data_TrafficLight trafficLight;
  if (SimOneAPI::GetTrafficLight(0, lightId, &trafficLight))//�õ����泡���еĽ�ͨ�Ƶ���ֵ
  {
    //ESimOne_TrafficLight_Status_Red = 1,
    //ESimOne_TrafficLight_Status_Green = 2,
    //ESimOne_TrafficLight_Status_Yellow = 3,
    if (trafficLight.status != ESimOne_TrafficLight_Status::ESimOne_TrafficLight_Status_Green) {
      //�����̵�
      return false;
    }
    //������̵ƣ����Ƶ�count�Ƿ�С��10
    else {
      //std::cout << "light status: " << trafficLight.status << std::endl;
      //std::cout << "trafficLight.countDown: " << trafficLight.countDown << std::endl;//����ʱ
      if (trafficLight.countDown != -1) {
        if (distance / speed < (trafficLight.countDown - 2) || trafficLight.countDown > 10) return true;
      } else if (trafficLight.countDown == -1) return true;
      else return false;

    }
  }
}

//��������������Ƿ���Ŀ��ĺ�̳���

SSD::SimString GetTargetSuccessorLane(const SSD::SimStringVector &successorLaneNameList,
                                      const SSD::SimVector<HDMapStandalone::MSignalValidity> &validities) {
  SSD::SimString targetSuccessorLaneId;
  for (auto &successorLane: successorLaneNameList) {
    HDMapStandalone::MLaneId id(successorLane);//��string��ʽ��idת��ɿ���ȡ��MLaneIdģʽ
    for (auto &validitie: validities) {
      if ((id.roadId == validitie.roadId && id.sectionIndex == validitie.sectionIndex &&
           id.sectionIndex == validitie.fromLaneId)
          || (id.roadId == validitie.roadId && id.sectionIndex == validitie.sectionIndex &&
              id.sectionIndex == validitie.toLaneId)) {
        targetSuccessorLaneId = successorLane;
        break;
      }
    }
  }
  return targetSuccessorLaneId;
}



//������ֹͣ�ߵľ����Ƿ��ڰ�ȫ��Χ��

bool IsInSafeDistance(const SSD::SimPoint3D &vehiclePos, const SSD::SimPoint3D &stopLine) {
  SSD::SimPoint2D vehiclePos2D(vehiclePos.x, vehiclePos.y);
  double safeDistance = UtilMath::distance(vehiclePos2D, SSD::SimPoint2D(stopLine.x, stopLine.y));
  //std::cout << "safeDistance " << safeDistance << std::endl;
  return int(safeDistance) > 20;
}

//�ж��Ƿ��Ѿ������źŵ�

bool PassedLight(const SSD::SimPoint3D &vehiclePos, const SSD::SimPoint3D &light, const SSD::SimString &laneId) {
  double s, t;
  bool found = SimOneAPI::GetLaneST(laneId, light, s, t);//�õ��ϰ����ڳ����ϵ�st����
  double sveh, tveh;
  found = SimOneAPI::GetLaneST(laneId, vehiclePos, sveh, tveh);//�õ������ڳ����ϵ�st����
  cout << "sveh: " << sveh << " s: " << s << endl;
  return sveh > s;//��������Ƿ��Ѿ�ͨ���ϰ���
}

bool Passedstopline(const SSD::SimPoint3D &vehiclePos, const SSD::SimPoint3D &stopline, const SSD::SimString &laneId) {
  double s, t;
  bool found = SimOneAPI::GetLaneST(laneId, stopline, s, t);//�õ��ϰ����ڳ����ϵ�st����
  double sveh, tveh;
  found = SimOneAPI::GetLaneST(laneId, vehiclePos, sveh, tveh);//�õ������ڳ����ϵ�st����
  //cout << "sveh: " << sveh << " s: " << s << endl;
  return sveh > s;//��������Ƿ��Ѿ�ͨ���ϰ���
}

//��ȡһ��ʵʱ���μ������(����st������)

SSD::SimPoint3DVector
GetdetectZone_realtime(const unique_ptr<SimOne_Data_Gps> &gps, const double &s_front, const double &s_back,
                       const double &t_left, const double &t_right) {
  SSD::SimPoint3DVector detectZone;
  SSD::SimPoint3D Zone, dir;
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  SSD::SimString laneid = GetNearMostLane(mainVehiclepos);
  double s, t, S_front, S_back, T_left, T_right;

  SimOneAPI::GetLaneST(laneid, mainVehiclepos, s, t);
  T_left = t + t_left;
  T_right = t + t_right;
  S_front = s + s_front;
  S_back = s + s_back;

  //�����������ĸ����st����ת����xyz���꣬������������㼯
  SimOneAPI::GetInertialFromLaneST(laneid, S_front, T_right, Zone, dir);
  detectZone.push_back(Zone);
  SimOneAPI::GetInertialFromLaneST(laneid, S_front, T_left, Zone, dir);
  detectZone.push_back(Zone);
  SimOneAPI::GetInertialFromLaneST(laneid, S_back, T_left, Zone, dir);
  detectZone.push_back(Zone);
  SimOneAPI::GetInertialFromLaneST(laneid, S_back, T_right, Zone, dir);
  detectZone.push_back(Zone);

  return detectZone;
}

//��ȡһ���̶����μ�����������st��ʵֵ��

SSD::SimPoint3DVector
GetdetectZone_fixed(const SSD::SimPoint3D &pos, const double &s_front, const double &s_back, const double &t_left,
                    const double &t_right) {
  SSD::SimPoint3DVector detectZone;
  SSD::SimPoint3D Zone, dir;
  SSD::SimString laneid = GetNearMostLane(pos);


  //�����������ĸ����st����ת����xyz���꣬������������㼯
  SimOneAPI::GetInertialFromLaneST(laneid, s_front, t_right, Zone, dir);
  detectZone.push_back(Zone);
  SimOneAPI::GetInertialFromLaneST(laneid, s_front, t_left, Zone, dir);
  detectZone.push_back(Zone);
  SimOneAPI::GetInertialFromLaneST(laneid, s_back, t_left, Zone, dir);
  detectZone.push_back(Zone);
  SimOneAPI::GetInertialFromLaneST(laneid, s_back, t_right, Zone, dir);
  detectZone.push_back(Zone);

  return detectZone;
}

//��ȡ��ǰ����s����
double GetSdistance(const unique_ptr<SimOne_Data_Gps> &gps, const obstaclestruct &obstacle,
                    const SSD::SimVector<long> &roadidlist) {
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  SSD::SimString laneid = GetNearMostLane(mainVehiclepos);
  SSD::SimString obstacleid = GetNearMostLane(obstacle.pt);

  HDMapStandalone::MLaneId id_car(laneid);
  HDMapStandalone::MLaneId id_obst(obstacleid);

  double s_distance;
  double s, s_obstacle, t, z;
  SimOneAPI::GetRoadST(laneid, mainVehiclepos, s, t, z);
  SimOneAPI::GetRoadST(obstacleid, obstacle.pt, s_obstacle, t, z);

  s_distance = s_obstacle - s;

  if (id_car.roadId <= id_obst.roadId) {
    HDMapStandalone::MLaneId current_id = id_car;
    while (current_id.roadId <= id_obst.roadId) {
      SSD::SimPoint3DVector centerline = GetLaneSample(current_id.ToString());
      double s_temp;
      SimOneAPI::GetRoadST(current_id.ToString(), centerline.back(), s_temp, t, z);
      s_distance += s_temp;

      // Get the next roadId
      auto it = std::find(roadidlist.begin(), roadidlist.end(), current_id.roadId);
      if (it != roadidlist.end() && std::next(it) != roadidlist.end()) {
        current_id.roadId = *(std::next(it));
      } else {
        break; // Break if unable to find the next roadId
      }
    }
  } else {
    HDMapStandalone::MLaneId current_id = id_car;
    while (current_id.roadId >= id_obst.roadId) {
      SSD::SimPoint3DVector centerline = GetLaneSample(current_id.ToString());
      double s_temp;
      SimOneAPI::GetRoadST(current_id.ToString(), centerline.back(), s_temp, t, z);
      s_distance -= s_temp;

      // Get the previous roadId
      auto it = std::find(roadidlist.begin(), roadidlist.end(), current_id.roadId);
      if (it != roadidlist.begin()) {
        current_id.roadId = *(std::prev(it));
      } else {
        break; // Break if unable to find the previous roadId
      }
    }
  }

  return s_distance;
}

//������к���Ƿ����˻����г�
bool CrosswalkOccupied(const SSD::SimPoint3DVector &crosswalk) {
  vector<obstaclestruct> allobstacles = GetObstacleList();//������е��ϰ���
  //cout << allobstacles.size() << endl;
  for (auto &obstacle: allobstacles) {
    if ((obstacle.type == 6 || obstacle.type == 4) &&  obstacle.speed != 0) {
      if (IsOccupied(obstacle.pt, crosswalk)) {
        return true;

      }
    }
  }
  return false;
}

//����Ƿ�ӽ�·��
bool DetectJunction(const unique_ptr<SimOne_Data_Gps> &gps) {
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  SSD::SimStringVector alllanes;
  SimOneAPI::GetNearLanesWithAngle(mainVehiclepos, 60, gps->oriZ, 30, alllanes);
  if (alllanes.size() >= 12) {
    return true;
  }
  return false;
}

//���·���Ƿ����ϰ���
bool DetectJunctionObstacle(const unique_ptr<SimOne_Data_Gps> &gps, SSD::SimPoint3D &stopline, double &distance) {
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  SSD::SimString laneid = GetNearMostLane(mainVehiclepos);
  if (DetectJunction(gps)) {
    SSD::SimPoint3DVector centerline = GetLaneSample(laneid);
    stopline = centerline.back();//�������������һ��Ϊֹͣ��

    //����·�ڼ������
    SSD::SimPoint3DVector junction_detectZone;
    double s, t, z, s_front, s_back, t_left, t_right;
    SSD::SimPoint3D Zone, dir2;

    SimOneAPI::GetRoadST(laneid, stopline, s, t, z);//���ͶӰ���st����
    t_left = t + 14.5;
    t_right = t - 4.5;
    s_front = s + 27.5;
    s_back = s + 8.5;

    //�����������ĸ����st����ת����xyz���꣬������������㼯
    SimOneAPI::GetInertialFromLaneST(laneid, s_front, t_right, Zone, dir2);
    junction_detectZone.push_back(Zone);
    SimOneAPI::GetInertialFromLaneST(laneid, s_front, t_left, Zone, dir2);
    junction_detectZone.push_back(Zone);
    SimOneAPI::GetInertialFromLaneST(laneid, s_back, t_left, Zone, dir2);
    junction_detectZone.push_back(Zone);
    SimOneAPI::GetInertialFromLaneST(laneid, s_back, t_right, Zone, dir2);
    junction_detectZone.push_back(Zone);

    /*cout << " 0: " << junction_detectZone[0].x << " , " << junction_detectZone[0].y << endl;
    cout << " 1: " << junction_detectZone[1].x << " , " << junction_detectZone[1].y << endl;
    cout << " 2: " << junction_detectZone[2].x << " , " << junction_detectZone[2].y << endl;
    cout << " 3: " << junction_detectZone[3].x << " , " << junction_detectZone[3].y << endl;*/

    vector<obstaclestruct> allobstacles = GetObstacleList();//������е��ϰ���
    //cout << allobstacles.size() << endl;
    for (auto &obstacle: allobstacles) {
      if (obstacle.type == 6 && obstacle.speed != 0) {
        if (IsOccupied(obstacle.pt, junction_detectZone)) {
          distance = UtilMath::distance(mainVehiclepos, obstacle.pt); //�����������ϰ���ľ���
          return true;
        }
      }
    }
  }
  return false;
}

//�������ı�־
bool DetectClosestSign(const unique_ptr<SimOne_Data_Gps> &gps, int &warningSignIndex, double &distance) {
  SSD::SimVector<HDMapStandalone::MSignal> TrafficSignList;
  SimOneAPI::GetTrafficSignList(TrafficSignList);
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  double minDist = std::numeric_limits<double>::max();
  int closestSignIndex = -1;
  std::cout << "TrafficSignListSize==" << TrafficSignList.size() << endl;
  for (int i = 0; i < TrafficSignList.size(); i++)//ͨ���������ҳ���ǰ�������Ľ�ͨ��־
  {

    auto &sign = TrafficSignList[i];
    double distanceSign = UtilMath::distance(mainVehiclepos, SSD::SimPoint3D(sign.pt.x, sign.pt.y, sign.pt.z));
    if (distanceSign < minDist) {
      minDist = distanceSign;
      closestSignIndex = i;
    }
  }
  if (closestSignIndex == -1) {
    printf("DetectClosestSign fail!\n");
    return false;

  }
  warningSignIndex = closestSignIndex;
  distance = minDist;
  return true;

}

//��⳵���뾶�ڵĵ����ٱ�־
bool DetectSpeedLimitSignInR(const SimOne_Data_Gps &gps, int &warningSignIndex, double r, double &limit_speed) {
  SSD::SimVector<HDMapStandalone::MSignal> TrafficSignList;
  SimOneAPI::GetTrafficSignList(TrafficSignList);
  const double kAlertDistance = r;
  const SSD::SimPoint2D gps2D(gps.posX, gps.posY);

  double minDist = std::numeric_limits<double>::max();
  int closestSignIndex = -1;

  for (auto &sign: TrafficSignList) {
    const double distanceSign = UtilMath::distance(gps2D, SSD::SimPoint2D(sign.pt.x, sign.pt.y));
    if (distanceSign < minDist) {
      minDist = distanceSign;
      closestSignIndex = &sign - &TrafficSignList[0];
    }
  }

  if (minDist > kAlertDistance) {
    return false;
  }
  warningSignIndex = closestSignIndex;
  const auto &targetSign = TrafficSignList[closestSignIndex];
  TrafficSignType sign = GetTrafficSignType(targetSign.type);//ȷ��Ϊ���ٷ�

  if (sign == TrafficSignType::SpeedLimit_Sign) {
    std::string speedLimitValue = targetSign.value.GetString();
    double speedLimit;
    std::istringstream ss(speedLimitValue);
    ss >> speedLimit;
    limit_speed = speedLimit/3.6;
    return true;
  }

  // if (sign == TrafficSignType::CrossWalk_Sign)......����

  return false;
}

//�����������ٱ�־��

bool DetectNearstSpeedLimitSign(const unique_ptr<SimOne_Data_Gps> &gps, double &limitspeed) {
  SSD::SimVector<HDMapStandalone::MSignal> TrafficSignList;
  SimOneAPI::GetTrafficSignList(TrafficSignList);
  float mainVehicleVel = sqrtf(pow(gps->velX, 2) + pow(gps->velY, 2));
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  double currentdistance;
  const double kAlertDistance = 40;
  int closestSignIndex = -1;
  if (DetectClosestSign(gps, closestSignIndex, currentdistance))//��ȡ����Ľ�ͨ��־
  {
    //cout << "i an in" << endl;
    auto &targetSign = TrafficSignList[closestSignIndex];
    TrafficSignType sign = GetTrafficSignType(targetSign.type);//ȷ����ͨ��־��type
    //cout <<"distance: "<< minDist << endl;
    if (sign == TrafficSignType::SpeedLimit_Sign) {

      double distance = UtilMath::distance(mainVehiclepos,
                                           SSD::SimPoint3D(targetSign.pt.x, targetSign.pt.y, targetSign.pt.z));
      if (distance > kAlertDistance)//����������־�������40������false
      {
        return false;
      }

      std::string speedLimitValue = targetSign.value.GetString();//�������ֵ���ַ�����ʽ��
      double distanceSign = distance;
      double speedLimit;
      std::stringstream ss;//���ַ���ת�ɸ�����
      ss << speedLimitValue;
      ss >> speedLimit;
      //cout << "value: "<<speedLimitValue <<" speedlimit: "<<speedLimit << endl;
      limitspeed = speedLimit / 3.6;//��������ֵ(m/s)
      //cout<<" speedlimit: " << limitspeed << endl;
      return true;

    }
    return false;
  }
  return false;
}

bool DetectObstacleAhead_zoneVersion(const unique_ptr<SimOne_Data_Gps> &gps, obstaclestruct &obstacle,
                                     double &currentdistance) {
  SSD::SimPoint3D MainVehiclePos(gps->posX, gps->posY, gps->posZ);
  std::vector<obstaclestruct> allObstacles = GetObstacleList();
  double minDist = std::numeric_limits<double>::max(); // ��ʼ����С����
  int obstacleClosestIndex = -1;
  double s_obstacle, t_obstacle;
  SSD::SimPoint3DVector detectZone = GetdetectZone_realtime(gps, 25, 0, -1.5, 1.5);

  for (unsigned int i = 0; i < allObstacles.size(); i++) {
    auto &curObstacle = allObstacles[i];
    double distanceSign = UtilMath::distance(MainVehiclePos,
                                             SSD::SimPoint3D(curObstacle.pt.x, curObstacle.pt.y, curObstacle.pt.z));

    if (IsOccupied(curObstacle.pt, detectZone) && distanceSign < minDist) {
      minDist = distanceSign;
      obstacleClosestIndex = i;
    }
  }

  if (obstacleClosestIndex == -1) {
    return false; // δ��⵽�ϰ���
  }

  obstacle = allObstacles[obstacleClosestIndex];
  currentdistance = UtilMath::distance(MainVehiclePos, SSD::SimPoint3D(obstacle.pt.x, obstacle.pt.y, obstacle.pt.z));
  return true; // �ɹ���⵽�ϰ���
}

//�õ�һ��ͬ����ǰ��������ϰ���
bool
DetectObstacleAhead(const unique_ptr<SimOne_Data_Gps> &gps, obstaclestruct &obstacle, const SSD::SimVector<long> &roadidlist,
                    double &currentdistance) {
  int obstacleClosestIndex = -1;
  double minDist = std::numeric_limits<double>::max();//��ʼ����С����

  SSD::SimPoint3D MainVehiclePos(gps->posX, gps->posY, gps->posZ);
  std::vector<obstaclestruct> allObstacles = GetObstacleList();

  long nextlane;
  SSD::SimString laneid = GetNearMostLane(MainVehiclePos), laneObs;
  HDMapStandalone::MLaneLink lanelink;
  SimOneAPI::GetLaneLink(laneid, lanelink);
  double SOfObs, SOfV;
  SOfV = GetS(MainVehiclePos, laneid);
  HDMapStandalone::MLaneId mlane(laneid);
  auto it = std::find(roadidlist.begin(), roadidlist.end(), mlane.roadId);
  if (it != roadidlist.end() && std::next(it) != roadidlist.end()) {
    nextlane = *std::next(it);
  } else {
    nextlane = mlane.roadId;
  }


  for (unsigned int i = 0; i < allObstacles.size(); i++) {
    obstacle = allObstacles[i];
    SOfObs = GetS(obstacle.pt, laneid);
    // std::cout << "SOfV == " << SOfV << endl;
    // std::cout << "SOfObs== " << SOfObs << endl;
    laneObs = GetNearMostLane(obstacle.pt);
    double distance =
        UtilMath::distance(SSD::SimPoint3D(MainVehiclePos), obstacle.pt);
    SSD::SimString obstacle_laneid = GetNearMostLane(obstacle.pt);
    HDMapStandalone::MLaneId id(obstacle_laneid);

    if (find(roadidlist.begin(), roadidlist.end(), id.roadId) !=
            roadidlist.end() &&
        (obstacle_laneid != lanelink.leftNeighborLaneName &&
         obstacle_laneid != lanelink.rightNeighborLaneName) &&
        obstacle.speed >
            0) // ͬʱ�ڹ滮·����road�У�ͬʱ��������lane�����ҳ���
    {
      HDMapStandalone::MLaneId mobslane(laneObs);
      if (nextlane != mlane.roadId) {
        if (SOfObs > SOfV || nextlane == mobslane.roadId) { // �ж��Ƿ���ǰ��
          if (distance < minDist) {
            minDist = distance;
            obstacleClosestIndex = i;
          }
        }
      } else {
        if (SOfObs > SOfV) { // �ж��Ƿ���ǰ��
          if (distance < minDist) {
            minDist = distance;
            obstacleClosestIndex = i;
          }
        }
      }
    }

    else if(obstacle.speed==0&&obstacle.ownerLaneId==laneid)
    {
      if (SOfObs > SOfV) { // �ж��Ƿ���ǰ��
        if (distance < minDist) {
          minDist = distance;
          obstacleClosestIndex = i;
        }
      }
    }
  }
  if (obstacleClosestIndex == -1) {
    return false;
  }
  obstacle = allObstacles[obstacleClosestIndex];
  currentdistance = minDist;
  return true;
}

//��ȡ���һ����
SSD::SimPoint3D GetTerminalPoint() {
  SimOne_Data_WayPoints WayPoints;
  SSD::SimPoint3D endPt;
  const char *MainVehicleId = "0";
  if (SimOneAPI::GetWayPoints(MainVehicleId, &WayPoints)) {
    int waySize = WayPoints.wayPointsSize;
    endPt.x = WayPoints.wayPoints[waySize - 1].posX;
    endPt.y = WayPoints.wayPoints[waySize - 1].posY;
    endPt.z = 0;
  }
  return endPt;
}

//��ȡ���̵��Լ���ͣ����


HDMapStandalone::MSignal GetTargetLight(const HDMapStandalone::MLaneId &id, const SSD::SimString &laneId,
                                        const SSD::SimVector<long> &roadIdList) {
  HDMapStandalone::MSignal light;
  SSD::SimVector<HDMapStandalone::MSignal> lightList;
  SimOneAPI::GetTrafficLightList(lightList);
  for (auto &item: lightList) {
    int num = 0;
    for (auto &ptValidities: item.validities) {
      auto it = std::find(roadIdList.begin(), roadIdList.end(), ptValidities.roadId);
      if (it != roadIdList.end()) {
        ++num;
      }
    }

    //current lane
    if (num >= 2) {
      light = item;
      break;
    }
  }
  return std::move(light);
}

SSD::SimPoint3D GetTragetStopLine(const HDMapStandalone::MSignal &light, const SSD::SimString &laneId) {
  SSD::SimVector<HDMapStandalone::MObject> stoplineList;
  SimOneAPI::GetStoplineList(light, laneId, stoplineList);
  if (stoplineList.size() == 0) {
    return SSD::SimPoint3D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
  }
  SSD::SimPoint3D pt, dir;
  SimOneAPI::GetLaneMiddlePoint(stoplineList[0].pt, laneId, pt, dir);
  cout << "stoplineList.pt==" << pt.x << "," << pt.y << endl;
  return pt;
}

//�������ĸ���ͨ����ص�ͣ����
bool DetectStopline(const unique_ptr<SimOne_Data_Gps> &gps, SSD::SimPoint3D &stopLine,
                    HDMapStandalone::MSignal &currentlight, HDMapStandalone::MObject &crosswalk, double &currentdistance) {


  bool getlight = false;
  SSD::SimPoint3DVector inputPoints;
  SSD::SimPoint3D MainVehiclePos(gps->posX, gps->posY, gps->posZ);
  inputPoints.push_back(MainVehiclePos);
  SSD::SimPoint3D endPt = GetTerminalPoint();
  SSD::SimString endlaneid = GetNearMostLane(endPt);
  SSD::SimVector<int> indexOfValidPoints;
  SSD::SimVector<long> roadIdList;
  SimOneAPI::Navigate(inputPoints, indexOfValidPoints, roadIdList);
  SSD::SimString laneId = GetNearMostLane(MainVehiclePos);
  HDMapStandalone::MLaneId id(laneId);

  HDMapStandalone::MSignal light = GetTargetLightNow(id, laneId, roadIdList,getlight);
  if (getlight) {
    stopLine = GetTragetStopLine(light, laneId);//���ͣ����
    double distance = UtilMath::distance(MainVehiclePos, stopLine);
    currentlight = light;
    SSD::SimVector <HDMapStandalone::MObject> crosswalklist;
    SimOneAPI::GetCrosswalkList(light, laneId, crosswalklist);//������к���б�
    crosswalk = crosswalklist.front();//�������к���ĵ�һ��
    std::cout<<"crosswalk.boundaryKnots 0  "<<crosswalk.boundaryKnots[0].x<<"  "<<crosswalk.boundaryKnots[0].y<<std::endl;
    std::cout<<"crosswalk.boundaryKnots 1  "<<crosswalk.boundaryKnots[1].x<<"  "<<crosswalk.boundaryKnots[1].y<<std::endl;
    std::cout<<"crosswalk.boundaryKnots 2  "<<crosswalk.boundaryKnots[2].x<<"  "<<crosswalk.boundaryKnots[2].y<<std::endl;
    std::cout<<"crosswalk.boundaryKnots 3  "<<crosswalk.boundaryKnots[3].x<<"  "<<crosswalk.boundaryKnots[3].y<<std::endl;
    currentdistance = distance;
    return true;
  }
  return false;
}

//���û���źŵ�ͣ����
bool DetectNolightStopline(const unique_ptr<SimOne_Data_Gps> &gps, SSD::SimPoint3D &stopline, SSD::SimPoint3DVector &crosswalk,
                           double &currentdistance) {
  SSD::SimVector<HDMapStandalone::MSignal> TrafficSignList;
  SimOneAPI::GetTrafficSignList(TrafficSignList);
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);

  double signdistance;
  int closestSignIndex = -1;
  if (DetectClosestSign(gps, closestSignIndex, signdistance))//��ȡ����Ľ�ͨ��־
  {
    auto &targetSign = TrafficSignList[closestSignIndex];
    TrafficSignType sign = GetTrafficSignType(targetSign.type);
    //�����������������ı�־ʶ��
    if (sign == -1977821062)//�����־����ͣ��
    {
      //cout << "i am in" << endl;
      stopline = {TrafficSignList[closestSignIndex].pt.x + 3.1, TrafficSignList[closestSignIndex].pt.y + 0.7, 0};
      currentdistance = UtilMath::distance(stopline, mainVehiclepos);
      //�õ������ͣ����־�İ�����
      SSD::SimPoint3D knot = {TrafficSignList[closestSignIndex].pt.x + 8.1,
                              TrafficSignList[closestSignIndex].pt.y - 3.2, 0};
      crosswalk.push_back(knot);
      knot = {TrafficSignList[closestSignIndex].pt.x + 8.1, TrafficSignList[closestSignIndex].pt.y + 15.1, 0};
      crosswalk.push_back(knot);
      knot = {TrafficSignList[closestSignIndex].pt.x + 4.1, TrafficSignList[closestSignIndex].pt.y + 15.1, 0};
      crosswalk.push_back(knot);
      knot = {TrafficSignList[closestSignIndex].pt.x + 4.1, TrafficSignList[closestSignIndex].pt.y - 3.2, 0};
      crosswalk.push_back(knot);

      cout << " 0: " << crosswalk[0].x << " , " << crosswalk[0].y << endl;
      cout << " 1: " << crosswalk[1].x << " , " << crosswalk[1].y << endl;
      cout << " 2: " << crosswalk[2].x << " , " << crosswalk[2].y << endl;
      cout << " 3: " << crosswalk[3].x << " , " << crosswalk[3].y << endl;

      return true;
    }
    return false;
  }
  return false;
}

//���·���Ƿ�ӵ�£��õ����³�����id��
bool IsJunctionCrowed(const unique_ptr<SimOne_Data_Gps> &gps, const SSD::SimVector<long> &roadidlist) {
  SSD::SimPoint3DVector centerline;
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  SSD::SimString laneid = GetNearMostLane(mainVehiclepos);

  HDMapStandalone::MLaneId id(laneid);
  HDMapStandalone::MLaneId next_id;
  HDMapStandalone::MLaneId towards_id;

  if (DetectJunction(gps)) {
    int index = find(roadidlist.begin(), roadidlist.end(), id.roadId) - roadidlist.begin();//�ҵ���ǰ��·��index
    GetValidSuccessor(id, roadidlist[index], roadidlist[index + 1], next_id);
    GetValidSuccessor(next_id, roadidlist[index + 1], roadidlist[index + 2], towards_id);//�õ����泵����id

    centerline = GetLaneSample(towards_id.ToString());//�õ�������

    //����25�׵ļ������
    SSD::SimPoint3D startline = centerline.front();//�������������һ��Ϊֹͣ��
    SSD::SimPoint3DVector crowed_detectZone;
    double s, t, s_front, s_back, t_left, t_right;
    SSD::SimPoint3D Zone, dir2;

    SimOneAPI::GetLaneST(towards_id.ToString(), startline, s, t);//���ͶӰ���st����
    t_left = t - 1.7;
    t_right = t + 1.8;
    s_front = s + 25;
    s_back = s;

    //�����������ĸ����st����ת����xyz���꣬������������㼯
    SimOneAPI::GetInertialFromLaneST(towards_id.ToString(), s_front, t_right, Zone, dir2);
    crowed_detectZone.push_back(Zone);
    SimOneAPI::GetInertialFromLaneST(towards_id.ToString(), s_front, t_left, Zone, dir2);
    crowed_detectZone.push_back(Zone);
    SimOneAPI::GetInertialFromLaneST(towards_id.ToString(), s_back, t_left, Zone, dir2);
    crowed_detectZone.push_back(Zone);
    SimOneAPI::GetInertialFromLaneST(towards_id.ToString(), s_back, t_right, Zone, dir2);
    crowed_detectZone.push_back(Zone);

    cout << " 0: " << crowed_detectZone[0].x << " , " << crowed_detectZone[0].y << endl;
    cout << " 1: " << crowed_detectZone[1].x << " , " << crowed_detectZone[1].y << endl;
    cout << " 2: " << crowed_detectZone[2].x << " , " << crowed_detectZone[2].y << endl;
    cout << " 3: " << crowed_detectZone[3].x << " , " << crowed_detectZone[3].y << endl;

    vector<obstaclestruct> allobstacles = GetObstacleList();//������е��ϰ���
    for (auto &obstacle: allobstacles) {
      if (obstacle.type == 6 && obstacle.speed <= 1) {
        if (IsOccupied(obstacle.pt, crowed_detectZone)) {
          return true;
        }
      }
    }
  }
  return false;
}

//����Ƿ���Ա��
bool DetectIsTurnable(const unique_ptr<SimOne_Data_Gps> &gps, const obstaclestruct &obstacle, SSD::SimString &turn,
                      SSD::SimString &state) {
  std::vector<obstaclestruct> allObstacles = GetObstacleList();
  double s, t, s_obstacle, t_obstacle;

  SSD::SimPoint3DVector detectzone;
  SSD::SimPoint3D mainVehiclepos(gps->posX, gps->posY, gps->posZ);
  SSD::SimPoint3D dir, testpoint;
  SSD::SimString currentlaneid = GetNearMostLane(mainVehiclepos);
  SSD::SimString turntolane;
  HDMapStandalone::MLaneLink lanelink;
  HDMapStandalone::MLaneType leftlanetype, rightlanetype;

  SimOneAPI::GetLaneLink(currentlaneid, lanelink);//��õ�ǰ����������Ϣ
  HDMapStandalone::MRoadMark left, right;

  SimOneAPI::GetRoadMark(mainVehiclepos, currentlaneid, left, right);
  SimOneAPI::GetLaneType(lanelink.leftNeighborLaneName, leftlanetype);
  SimOneAPI::GetLaneType(lanelink.rightNeighborLaneName, rightlanetype);
  SimOneAPI::GetLaneST(currentlaneid, mainVehiclepos, s, t);
  SimOneAPI::GetLaneST(currentlaneid, obstacle.pt, s_obstacle, t_obstacle);

  double s_front = s_obstacle + 5;
  double s_back = s - 5;

  if (lanelink.leftNeighborLaneName.Empty() == 0 && leftlanetype == HDMapStandalone::MLaneType::driving &&
      left.type == HDMapStandalone::ERoadMarkType::broken) {
    turntolane = lanelink.leftNeighborLaneName;
    SSD::SimPoint3D changeToPoint;
    SimOneAPI::GetLaneMiddlePoint(obstacle.pt, turntolane, changeToPoint, dir);

    detectzone = GetdetectZone_fixed(changeToPoint, s_front, s_back, -1.75, 1.75);//�󳵵������

    if (!IsChangeLaneOccupied(allObstacles, detectzone)) {
      turn = "left";
      return true;
    } else {
      state = "wait";
    }
  } else if (lanelink.rightNeighborLaneName.Empty() == 0 && rightlanetype == HDMapStandalone::MLaneType::driving &&
             right.type == HDMapStandalone::ERoadMarkType::broken) {
    turntolane = lanelink.rightNeighborLaneName;
    SSD::SimPoint3D changeToPoint;
    SimOneAPI::GetLaneMiddlePoint(obstacle.pt, turntolane, changeToPoint, dir);

    detectzone = GetdetectZone_fixed(changeToPoint, s_front, s_back, -1.75, 1.75);//�󳵵������

    if (!IsChangeLaneOccupied(allObstacles, detectzone)) {
      turn = "right";
      return true;
    } else {
      state = "wait";
    }
  } else {
    state = "cant_change";
    return false;
  }
  return false;
}


bool
IsInChangeLane(const SSD::SimPoint3DVector &targetPath, SimOne_Data_Gps *pGps, HDMapStandalone::MLaneLink &lanelink, string &turnlight) {
  SSD::SimString lanenow, laneforward;
  std::vector<float> pts;
  for (size_t i = 0; i < targetPath.size(); ++i) {
    pts.push_back(pow((pGps->posX - (float) targetPath[i].x), 2) + pow((pGps->posY - (float) targetPath[i].y), 2));
  }

  size_t index = std::min_element(pts.begin(), pts.end()) - pts.begin();
  size_t forwardIndex = 0;
  float minProgDist = 3.f;
  float progTime = 0.8f;
  float mainVehicleSpeed = sqrtf(pGps->velX * pGps->velX + pGps->velY * pGps->velY + pGps->velZ * pGps->velZ);
  float progDist = mainVehicleSpeed * progTime > minProgDist ? mainVehicleSpeed * progTime : minProgDist;

  for (; index < targetPath.size(); ++index) {
    forwardIndex = index;
    float distance = sqrtf(
        ((float) pow(targetPath[index].x - pGps->posX, 2) + pow((float) targetPath[index].y - pGps->posY, 2)));
    if (distance >= progDist) {
      break;
    }
  }
  laneforward = GetNearMostLane(targetPath[forwardIndex]);
  lanenow = GetNearMostLane(SSD::SimPoint3D(pGps->posX, pGps->posY, pGps->posZ));
  if (lanenow != laneforward) {
    if (laneforward == lanelink.leftNeighborLaneName) {
      turnlight = "L";
      return true;
    } else if (laneforward == lanelink.rightNeighborLaneName) {
      turnlight = "R";
      return true;
    }

  }
  return false;
}


//�������
SSD::SimPoint3DVector ChangeLanePathWithLane(const SSD::SimPoint3D& mainVehiclepos, double speed, const SSD::SimString &changetolane,
                                             SSD::SimPoint3D &changeToPoint) {
  double s, t, s_changePath, t_changePath;
  SSD::SimPoint3DVector changelane_path;
  SSD::SimPoint3D foward_point, dir;
  SSD::SimString currentlaneid = GetNearMostLane(mainVehiclepos);
  SimOneAPI::GetLaneST(currentlaneid, mainVehiclepos, s, t);
  s_changePath = s + std::max(speed * 3,5.);
  SimOneAPI::GetInertialFromLaneST(currentlaneid, s_changePath, t, foward_point, dir);
  SimOneAPI::GetLaneMiddlePoint(foward_point, changetolane, changeToPoint, dir);
  changelane_path=ChangeLanePathWithPoint(mainVehiclepos, changeToPoint);
  return changelane_path;
}
//�ϰ���������
SSD::SimPoint3DVector ChangeLanePathWithObstacle(const SSD::SimPoint3D& mainVehiclepos, const SSD::SimString &changetolane,SSD::SimPoint3D &obstaclepos,SSD::SimPoint3D &TurnToPoint){
  SSD::SimPoint3DVector changelane_path;
  SSD::SimPoint3D dir;
  SimOneAPI::GetLaneMiddlePoint(obstaclepos, changetolane, TurnToPoint, dir);
  changelane_path=ChangeLanePathWithPoint(mainVehiclepos, TurnToPoint);
  return changelane_path;
}
//��ӡ·���㼯��
void PrintTargetPath(SSD::SimPoint3DVector &targetpath) {
  cout << "targetpath.size()==  " << targetpath.size() << endl;
  for (int i = 0; i < targetpath.size(); i += 1) {
    std::cout << "Point[" << i << "]==  " << targetpath[i].x << "," << targetpath[i].y << "," << targetpath[i].z
              << endl;
  }
}

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  std::cout.tie(nullptr);

  /* ------------------------ ��ʼ������ ------------------------ */

  std::unique_ptr<SimOne_Data_Gps> gpsPtr = std::make_unique<SimOne_Data_Gps>();
  std::unique_ptr<SimOne_Data_Obstacle> obstaclesPtr = std::make_unique<SimOne_Data_Obstacle>();
  std::unique_ptr<SimOne_Data_Signal_Lights> pSignalLights = std::make_unique<SimOne_Data_Signal_Lights>();//�źŵ�
  const char* MainVehicleId = "0"; // ������ID
  bool inAEBState = false;
  bool isJoinTimeLoop = true; // �Ƿ����ʱ��ѭ��
  bool firstFrame = false;
  bool secondFrame = false;
  SimOneAPI::InitSimOneAPI(MainVehicleId, isJoinTimeLoop);
  SimOneAPI::SetDriverName(MainVehicleId, "AutoDrive");
  SimOneAPI::InitEvaluationServiceWithLocalData(MainVehicleId);
  int timeout = 20;

  /* ����Map */
  while (true) {
    if (SimOneAPI::LoadHDMap(timeout)) {
      SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Information, "HDMap Information Loaded");
      break;
    }
    SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Information, "HDMap Information Loading...");
  }

  /* ��ʼ������ */
  while (true) {
    SimOneAPI::GetGps(MainVehicleId, gpsPtr.get());
    if ((gpsPtr->timestamp > 0)) {
      printf("SimOne Initialized\n");
      break;
    }
    printf("SimOne Initializing...\n");
  }

  /* ʵ�����۷����ʼ�� */
  SimOneAPI::InitEvaluationServiceWithLocalData(MainVehicleId);//ʵ�����۷����ʼ��

  /* ����ȫ�ֱ��� */
  double InitialSteering;
  double slow_speed = 10;

  /* ����PID */
  PIDController SlowSpeed, HighSpeed;
  SlowSpeed.setPID(1, 0, 0, 0.1);
  SlowSpeed.setOutputLimits(-5, 5);
  HighSpeed.setPID(0.8, 0.0001, 2, 0.1);
  HighSpeed.setOutputLimits(-5, 5);

  /* �������,�յ�
     * δʹ��: turnToPoint */
  SSD::SimPoint3D startPt, endPt = GetTerminalPoint();
  SSD::SimPoint3D TurnToPoint;

  /* ����Ŀ�������,��ֹ������
     * δʹ��: detectZone */
  SSD::SimPoint3DVector targetpath, StartAndEnd;
  SSD::SimPoint3DVector detectzone;

  /* ת���·��ID */
  SSD::SimString ChangeToLane_ID;

  /* ������ֵ */
  double throttle;

  string RunState = "Start!";

  bool isNearSection = false;

  /* ͣ��,���,���������α��+���٣�,�����ϰ���,ͣ��������ײ,���������󲿷������,�źŵ�ͣ��,��·��ͣ��,���� */

  /* ��ʼ�� */
  pSignalLights->signalLights = 0;// һ��ʼ�����ò����κ��źŵ�
  SSD::SimPoint3D mainVehiclePos = { gpsPtr->posX, gpsPtr->posY, gpsPtr->posZ };
  SSD::SimString currentLaneID = GetNearMostLane(mainVehiclePos);

  /* ��ó�����ǰλ�� */
  if (SimOneAPI::GetGps(MainVehicleId, gpsPtr.get()))
  {
    startPt.x = gpsPtr->posX;
    startPt.y = gpsPtr->posY;
    startPt.z = gpsPtr->posZ;
    StartAndEnd.push_back(startPt);
  }

  /* ��ù滮·���ĵ�·�б� */
  SSD::SimVector<long> naviRoadIdList = GetNavigateRoadIdList(startPt, endPt);//��ù滮·���ĵ�·�б�
  StartAndEnd.push_back(endPt);
  cout << " naviroadsize: " << naviRoadIdList.size() << endl;

  /* ��ʼʱ���ʼ�� */
  auto startTime = std::chrono::steady_clock::now();

  /* ------------------------ ������ʻ���� ------------------------ */

  while (true) {
    int frame = SimOneAPI::Wait();

    /* ��ȡ����������� */
    if (SimOneAPI::GetCaseRunStatus() == ESimOne_Case_Status::ESimOne_Case_Status_Stop) {
      SimOneAPI::SaveEvaluationRecord();
      break;
    }

    /* ��õ�ǰ������Ϣ*/
    SimOneAPI::GetGps(MainVehicleId, gpsPtr.get());

    /* ��õ�����������Ϣ */
    SimOneAPI::GetGroundTruth(MainVehicleId, obstaclesPtr.get());
    SSD::SimPoint3DVector crosswalknots;
    SSD::SimVector<int> indexOfValidPoints;
    mainVehiclePos = { gpsPtr->posX, gpsPtr->posY, gpsPtr->posZ };// ���������λ��
    currentLaneID = GetNearMostLane(mainVehiclePos);
    static SSD::SimString turn;// ���ת�������ڴ�ת���
    SSD::SimString state;// ����ܷ���
    HDMapStandalone::MLaneLink lanelink;
    HDMapStandalone::MLaneType leftlanetype, rightlanetype;
    SimOneAPI::GetLaneLink(currentLaneID, lanelink);
    obstaclestruct Obstacle_ahead;
    std::vector<obstaclestruct> AllObstacle;
    double mainVehicleSpeed = UtilMath::calculateSpeed(gpsPtr->velX, gpsPtr->velY, gpsPtr->velZ);//����������ٶ�(m/s)
    double mainVehicleAccel = UtilMath::calculateSpeed(gpsPtr->accelX, gpsPtr->accelY, gpsPtr->accelZ);//���������ʵʱ���ٶ�
    double ObsDistance = 1000;
    double limitspeed;
    static double maxspeed = 30 / 3.6;
    double minspeed = 11 / 3.6;
    double accelerate;
    double stop_distance = 5.1;// ͣ������
    double nothing_distance = 1000;
    double obstacleAhead_distance = 1000;// ͬ�����ϰ������
    double light_stopline_distance = 1000;// �źŵ��ϰ������
    double nolight_stopline_distance = 1000;// ���źŵ��ϰ������
    double junction_o_distance = 1000; // ·���ϰ���

    /* ����ͣ���� */
    HDMapStandalone::MObject crosswalk;
    HDMapStandalone::MSignal stopsign;
    HDMapStandalone::MSignal light;
    SSD::SimPoint3D stopline = GetTragetStopLine(GetTargetLight(HDMapStandalone::MLaneId(currentLaneID), currentLaneID, naviRoadIdList), currentLaneID);
    SSD::SimPoint3D light_stopline;
    SSD::SimPoint3D nolight_stopline;
    SSD::SimPoint3D junction_stopline;
    double StopLineDistance = UtilMath::distance(mainVehiclePos, stopline);

    /* ����Ƿ��г��ٱ�־ */
    if (DetectNearstSpeedLimitSign(gpsPtr, limitspeed)) {
      maxspeed = limitspeed;//����ٶ�����Ϊ�����ٶ�
      cout << "maxspeed" << maxspeed << endl;
    }
    DetectStopline(gpsPtr, light_stopline, light, crosswalk, light_stopline_distance);//�������Ľ�ͨ�ƹ���ͣ����
    DetectNolightStopline(gpsPtr, nolight_stopline, crosswalknots, nolight_stopline_distance);//��ȡ������޽�ͨ��ͣ����
    std::cout << nolight_stopline.x;
    std::cout << nolight_stopline.y;
    std::cout << nolight_stopline.z;
    DetectJunctionObstacle(gpsPtr, junction_stopline, junction_o_distance);//��ȡ·�ڴ����ϰ���
    //��ȡͬ����ǰ���ϰ���
    bool ObsExist = DetectObstacleAhead(gpsPtr, Obstacle_ahead, naviRoadIdList, obstacleAhead_distance);//��ȡͬ������ǰ���ϰ���
    if (ObsExist) {//���ǰ���ϰ�����ڣ���ӡ���ϰ������������debug
      cout << "Obstacle Exist!" << endl;
      ObsDistance = UtilMath::distance(mainVehiclePos, Obstacle_ahead.pt);
      std::cout << "Speed" << Obstacle_ahead.speed << endl;
      std::cout << "distance" << ObsDistance << endl;
      std::cout<<"obslane"<<Obstacle_ahead.ownerLaneId.GetString()<<endl;
    }
    AllObstacle.clear();
    AllObstacle = GetObstacleList();//��ȡ�����ϰ�����ڲ��Ե�
    std::cout << AllObstacle.size() << endl;
    //IsCollision(gpsPtr,obstaclesPtr);

    //����Ƿ���·��
    if(light_stopline_distance<40 ||(nolight_stopline_distance<40 )||StopLineDistance<=40){
      std::cout<<"in NearIntersection"<<endl;
      RunState="NearIntersection";
      isNearSection=true;
    } else if(light_stopline_distance > 40 && (nolight_stopline_distance > 40 ) && StopLineDistance > 40 && isNearSection)
    {
      RunState = "Follow";
      isNearSection = false;
    }

    /* ���Ƴ���,���ɱ��·���� */
    if (RunState == "Start!") {
      SimOneAPI::GenerateRoute(StartAndEnd, indexOfValidPoints, targetpath);//��ó�ʼ�Ĺ滮·���ĵ㼯
      PrintTargetPath(targetpath);
      RunState = "Follow";            //Ĭ�Ͻ��ȸ���
    }
    /* ������������Ҫ�� */
    else if (RunState == "Follow")    //������������Ҫ��
    {
      if (ObsExist) {
        if (ObsDistance < 40) {
          if (Obstacle_ahead.speed>0.5 && Obstacle_ahead.speed < 2 && Obstacle_ahead.type == 6) {
            if(DetectIsTurnable(gpsPtr, Obstacle_ahead, turn, state)){
              RunState = "ChangeLaneStart";//����ǳ����Ϊ���}
            }
          }

          else if (Obstacle_ahead.speed <= 0.5&&secondFrame){
            if(DetectIsTurnable(gpsPtr, Obstacle_ahead, turn, state)){
              RunState = "ObstacleAvoid";//��������
            }
            else {
              if(Obstacle_ahead.type==6){
                RunState="CarAEB";
              }
              else RunState="ObstacleAEB";

            }
          }
        }
        /* �����ϰ����룬ִ�в�ͬ���� */
        if (ObsDistance > 60) SlowSpeed.Setpoint(maxspeed);
        else if ((ObsDistance < 60) && (ObsDistance > mainVehicleSpeed * 1.5 + 15))
          SlowSpeed.Setpoint(std::min(maxspeed, Obstacle_ahead.speed * 1.1));
        else if ((ObsDistance <= mainVehicleSpeed * 1.5 + 15) &&
                 (ObsDistance > mainVehicleSpeed * 1 + 10))
          SlowSpeed.Setpoint(std::min(maxspeed, Obstacle_ahead.speed));
        else if (ObsDistance <= mainVehicleSpeed * 1 + 10) SlowSpeed.Setpoint(Obstacle_ahead.speed * 0.6);
        if (ObsDistance <= 5)  SlowSpeed.Setpoint(-10);
      }
      else SlowSpeed.Setpoint(maxspeed);
      if(secondFrame== false){
        SlowSpeed.Setpoint(11/3.6);
      }
      throttle = SlowSpeed.compute(mainVehicleSpeed);
    }
    /* ��ʼ��� */
    else if (RunState == "ChangeLaneStart") {//��ʼ���
      cout << "turn" << turn.GetString() << endl;
      if (turn == "right") {//��ת��ת
        ChangeToLane_ID = lanelink.rightNeighborLaneName;
        cout << "ChangeToLane_ID" << ChangeToLane_ID.GetString() << endl;
        pSignalLights->signalLights = ESimOne_Signal_Light_RightBlinker;//���
      }
      else if (turn == "left") {
        ChangeToLane_ID = lanelink.leftNeighborLaneName;
        cout << "ChangeToLane_ID" << ChangeToLane_ID.GetString() << endl;
        pSignalLights->signalLights = ESimOne_Signal_Light_LeftBlinker;
      }
      targetpath.clear();//����path
      SSD::SimPoint3DVector Path1, Path2;
      Path1 = ChangeLanePathWithLane(mainVehiclePos, mainVehicleSpeed, ChangeToLane_ID, TurnToPoint);
      for (auto& point : Path1) {//�����·��
        targetpath.push_back(point);
      }
      StartAndEnd.clear();
      StartAndEnd.push_back(TurnToPoint);
      StartAndEnd.push_back(endPt);
      SimOneAPI::GenerateRoute(StartAndEnd, indexOfValidPoints, Path2);
      naviRoadIdList = GetNavigateRoadIdList(TurnToPoint, endPt);
      for (auto& point : Path2) {//�����·�ߵ�״̬
        targetpath.push_back(point);
        cout << "point.x:" << point.x << "point.y:" << point.y << endl;
      }
      PrintTargetPath(targetpath);
      if (mainVehicleSpeed <= 1) {
        SlowSpeed.Setpoint(1.5);
        throttle = SlowSpeed.compute(mainVehicleSpeed);
      }
      RunState = "InChangeLane";//ת��Ϊ�����
    }
    else if (RunState == "ObstacleAvoid") {
      //����
      if (mainVehicleSpeed < 1.5) {
        SlowSpeed.Setpoint(1.5);
      }
      else if (mainVehicleSpeed > 1.5 && mainVehicleSpeed < 5) {
        SlowSpeed.Setpoint(mainVehicleSpeed * 0.8);
      }
      else SlowSpeed.Setpoint(4);
      //�����ϰ���λ�ÿ�ʼ���
      cout << "turn" << turn.GetString() << endl;//�������ı�ת
      if (turn == "right") {
        ChangeToLane_ID = lanelink.rightNeighborLaneName;
        cout << "ChangeToLane_ID" << ChangeToLane_ID.GetString() << endl;
        pSignalLights->signalLights = ESimOne_Signal_Light_RightBlinker;//���
      }
      else if (turn == "left") {
        ChangeToLane_ID = lanelink.leftNeighborLaneName;
        cout << "ChangeToLane_ID" << ChangeToLane_ID.GetString() << endl;
        pSignalLights->signalLights = ESimOne_Signal_Light_LeftBlinker;
      }
      targetpath.clear();//����path��ͬ��
      SSD::SimPoint3DVector Path2;
      targetpath = ChangeLanePathWithObstacle(mainVehiclePos, ChangeToLane_ID, Obstacle_ahead.pt, TurnToPoint);
      StartAndEnd.clear();
      StartAndEnd.push_back(TurnToPoint);
      StartAndEnd.push_back(endPt);
      SimOneAPI::GenerateRoute(StartAndEnd, indexOfValidPoints, Path2);
      naviRoadIdList = GetNavigateRoadIdList(TurnToPoint, endPt);
      for (auto& point : Path2) {
        targetpath.push_back(point);
        cout << "point.x:" << point.x << "point.y:" << point.y << endl;
      }
      PrintTargetPath(targetpath);
      RunState = "InChangeLane";
    }
    else if (RunState == "InChangeLane") {
      if (mainVehicleSpeed < 1.5) {
        SlowSpeed.Setpoint(1.5);
      }
      else SlowSpeed.Setpoint(mainVehicleSpeed);
      throttle = SlowSpeed.compute(mainVehicleSpeed);
      if (UtilMath::distance(mainVehiclePos, TurnToPoint) < 3) {
        pSignalLights->signalLights = 0;
        RunState = "Follow";
      }
    }
    else if (RunState == "NearIntersection"||isNearSection) {//��·�ڵ�״̬��ͨ������·��·�����ж�
      std::cout<<"(RunState == \"NearIntersection\"||isNearSection)"<<std::endl;
      if (StopLineDistance < nolight_stopline_distance) {
        std::cout<<"(StopLineDistance < nolight_stopline_distance)"<<std::endl;
        //��·���к��̵ƣ���ʼ�ж��Ƿ����ͨ����
        //�̵�ʱ��������û�еƣ���ȡǰ����·��Ϣ�������
        //ͣ�º�Ϳ�ʼ�Ⱥ���ˣ����꿪ʼ��������֪������·��
        if (IsGreenLight(light.id, currentLaneID, light, mainVehicleSpeed, StopLineDistance) &&
            !CrosswalkOccupied(crosswalk.boundaryKnots) && !IsJunctionCrowed(gpsPtr, naviRoadIdList)) {
          std::cout<<"IsGreenLight(light.id, currentLaneID,"<<std::endl;
          if (ObsDistance > 40) SlowSpeed.Setpoint(maxspeed);
          else if ((ObsDistance < 40) && (ObsDistance > mainVehicleSpeed * 1.5 + 15))
            SlowSpeed.Setpoint(std::min(maxspeed, Obstacle_ahead.speed * 1.1));
          else if ((ObsDistance <= mainVehicleSpeed * 1.5 + 15) &&(ObsDistance > mainVehicleSpeed * 1 + 10))
            SlowSpeed.Setpoint(std::min(maxspeed, Obstacle_ahead.speed * 1.00));
          else if (ObsDistance <= mainVehicleSpeed * 1 + 10)
            SlowSpeed.Setpoint(Obstacle_ahead.speed * 0.5);
          if (ObsDistance <= 5)
            SlowSpeed.Setpoint(-10);
          //                    if (Passedstopline(mainVehiclePos, stopline, currentLaneID)) {
          //                        RunState = "Follow";
          //                    }
        }
        /* ���̵ƻ�ʱ�䲻����ж��������е��ϣ�ͣ�� */
        else {
          cout<<"else"<<endl;
          if (ObsExist) {
            if ((ObsDistance <= 30 || StopLineDistance <= stop_distance + 30) &&
                (ObsDistance > 20 || StopLineDistance > stop_distance + 15))
              SlowSpeed.Setpoint(std::min(8.0, Obstacle_ahead.speed * 1.1));
            else if ((ObsDistance <= 20 || StopLineDistance <= stop_distance + 15) &&
                     (ObsDistance > 12 || StopLineDistance > stop_distance + 5))
              SlowSpeed.Setpoint(std::min(3.0, Obstacle_ahead.speed));
            else if (ObsDistance <= 12 || StopLineDistance <= stop_distance + 5) {
              SlowSpeed.Setpoint(-10);
              //std::cout << "���Ѿ���0��" << std::endl;
            }
          }
          else {
            std::cout << "102938" << std::endl;
            if ((StopLineDistance <= stop_distance + 20) &&
                (StopLineDistance > stop_distance + 10))
              SlowSpeed.Setpoint(8.0);
            else if ((StopLineDistance <= stop_distance + 10) &&
                     (StopLineDistance > stop_distance+1.5))
              SlowSpeed.Setpoint(3.0);
            else if (StopLineDistance <= stop_distance+1.5)SlowSpeed.Setpoint(-10);
          }
        }
      }
      else if (StopLineDistance > nolight_stopline_distance) {//û��·���������һ���������⣬��û����
        //                bool flag1 = CrosswalkOccupied(crosswalknots);
        //                bool flag2 = IsJunctionCrowed(gpsPtr, naviRoadIdList);
        //                std::cout << "flag1:" << flag1;
        //                std::cout << "flag2:" << flag2;
        if (!CrosswalkOccupied(crosswalknots) && !IsJunctionCrowed(gpsPtr, naviRoadIdList)) {
          std::cout << "station111" << std::endl;
          if (ObsDistance > 60) SlowSpeed.Setpoint(maxspeed);
          else if ((ObsDistance < 60) && (ObsDistance > mainVehicleSpeed * 1.5 + 15))
            SlowSpeed.Setpoint(std::min(maxspeed, Obstacle_ahead.speed * 1.1));
          else if ((ObsDistance <= mainVehicleSpeed * 1.5 + 15) &&
                   (ObsDistance > mainVehicleSpeed * 1 + 10))
            SlowSpeed.Setpoint(std::min(maxspeed, Obstacle_ahead.speed));
          else if (ObsDistance <= mainVehicleSpeed * 1 + 10) SlowSpeed.Setpoint(Obstacle_ahead.speed * 0.9);
          //                    if (Passedstopline(mainVehiclePos, nolight_stopline, currentLaneID)) {
          //                        RunState = "Follow";
          //                    }
          /*
          else {
              oFile << "6 " << endl;
              if ((std::min(ObsDistance, nolight_stopline_distance) <= 30) &&
                  (std::min(ObsDistance, nolight_stopline_distance) > 20))
                  SlowSpeed.setSetpoint(std::min(8.0, Obstacle_ahead.speed));
              else if ((std::min(ObsDistance, nolight_stopline_distance) <= 20) &&
                  (std::min(ObsDistance, nolight_stopline_distance) > 10))
                  SlowSpeed.setSetpoint(std::min(3.0, Obstacle_ahead.speed));
              else if (std::min(ObsDistance, nolight_stopline_distance) <= 10) SlowSpeed.setSetpoint(0);

          }*/
        }
        else {
          std::cout << "success" << std::endl;
          if ((std::min(ObsDistance, nolight_stopline_distance) <= 30) &&
              (std::min(ObsDistance, nolight_stopline_distance) > 20))
            SlowSpeed.Setpoint(std::min(7.0, Obstacle_ahead.speed));
          else if ((std::min(ObsDistance, nolight_stopline_distance) <= 20) &&
                   (std::min(ObsDistance, nolight_stopline_distance) > 15))
            SlowSpeed.Setpoint(std::min(2.0, Obstacle_ahead.speed));
          else if (std::min(ObsDistance, nolight_stopline_distance) <= 15) SlowSpeed.Setpoint(-10);

        }
      }
      throttle = SlowSpeed.compute(mainVehicleSpeed);
    }
    else if(RunState=="CarAEB"){
      if(DetectIsTurnable(gpsPtr, Obstacle_ahead, turn, state)){
        RunState = "ObstacleAvoid";//��������
      }
      if(Obstacle_ahead.speed>0.5){
        RunState="Follow";
      }
      if (ObsDistance <= 40 && ObsDistance > 30 ) SlowSpeed.Setpoint(std::min(8.0,mainVehicleSpeed));
      else if (ObsDistance <= 30 && ObsDistance > 20) SlowSpeed.Setpoint(std::min(3.0, mainVehicleSpeed));
      else if (ObsDistance <= 20) SlowSpeed.Setpoint(-10);
      throttle = SlowSpeed.compute(mainVehicleSpeed);
    }
    else if(RunState=="ObstacleAEB"){
      if(DetectIsTurnable(gpsPtr, Obstacle_ahead, turn, state)){
        RunState = "ObstacleAvoid";//��������
      }
      if(obstacleAhead_distance>=40){
        RunState="Follow";
      }
      if (ObsDistance <= 40 && ObsDistance > 20 ) SlowSpeed.Setpoint(std::min(8.0,mainVehicleSpeed));
      else if (ObsDistance <= 20 && ObsDistance > 10) SlowSpeed.Setpoint(std::min(3.0, mainVehicleSpeed));
      else if (ObsDistance <= 10) SlowSpeed.Setpoint(-10);
      throttle = SlowSpeed.compute(mainVehicleSpeed);
    }


    double alfa, ld;
    size_t index, forwardIndex;
    InitialSteering = UtilDriver::calculateSteering( targetpath, gpsPtr.get(),forwardIndex);
    if (forwardIndex > targetpath.size() - 3) {
      RunState = "Finish";
    }
    //if(RunState == "Finishing" && SimOneAPI::GetCaseRunStatus() != ESimOne_Case_Status::ESimOne_Case_Status_Stop)



    /* ���㷽������ֵ */
    if (!targetpath.empty()) {//�ж��Ƿ���·���㣬��ֹ�������
      InitialSteering = UtilDriver::calculateSteering(targetpath, gpsPtr.get(),forwardIndex);
    }
    else {
      SimOneAPI::GenerateRoute(StartAndEnd, indexOfValidPoints, targetpath);//��ó�ʼ�Ĺ滮·���ĵ㼯
      PrintTargetPath(targetpath);
      InitialSteering = 0;
    }

    if(RunState == "Finishing"||RunState == "Finish"){
      InitialSteering = 0;
    }

    UtilDriver::setDriver(gpsPtr->timestamp, throttle, 0, float(InitialSteering));//��������
    SimOneAPI::SetSignalLights(MainVehicleId, pSignalLights.get());

    /* ------------------------ ʱ����㲿�� ------------------------ */

    /* �����Ѿ���ȥ��ʱ�� */
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

    /* ����Ƿ��Ѿ���ȥ��5�� */
    if (elapsedSeconds >= 5) {
      /* ������ʼʱ�� */
      startTime = currentTime;
      if (RunState != "InChangeLane") {
        StartAndEnd.clear();
        StartAndEnd.push_back(mainVehiclePos);
        endPt = GetTerminalPoint();
        StartAndEnd.push_back(endPt);
        SimOneAPI::GenerateRoute(StartAndEnd, indexOfValidPoints, targetpath);
        naviRoadIdList = GetNavigateRoadIdList(startPt, endPt);
        std::cout << "rebuild targetpath" << endl;
      }
    }

    /* Check */
    SlowSpeed.Print();

    /* ��Ҫprint�Ĳ��Դ��� */
    std::cout << "throttle" << throttle << endl;
    std::cout << "ObstaclePs" << Obstacle_ahead.pt.x << " " << Obstacle_ahead.pt.y << " " << Obstacle_ahead.pt.z << std::endl;
    std::cout << "TurnToPoint" << TurnToPoint.x << " " << TurnToPoint.y << " " << TurnToPoint.z << std::endl;
    std::cout << "mainVehicleSpeed==" << mainVehicleSpeed << "m/s" << endl;
    std::cout << "position==" << mainVehiclePos.x << "," << mainVehiclePos.y << endl;
    std::cout << "laneID==" << currentLaneID.GetString() << endl;
    std::cout << "light_stopline_distance==" << light_stopline_distance << endl;
    std::cout << "nolight_stopline_distance==" << nolight_stopline_distance << endl;
    std::cout << "Runstate::" << RunState << endl;
    std::cout << "obstaclespeed==" << Obstacle_ahead.speed << endl;


    if(firstFrame){
      secondFrame= true;
    }
    firstFrame= true;

    /* ���ó���״̬���ȴ���һ֡ */
    SimOneAPI::NextFrame(frame);
    // std::this_thread::sleep_for(std::chrono::microseconds (100000));
  }
  return 0;
}


