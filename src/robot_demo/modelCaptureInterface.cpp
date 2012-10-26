#include "modelCaptureInterface.hpp"
#include "rosUtils.hpp"
//TODO: use splitted code
#include "../../opencv_candidate/src/rgbd/samples/model_capture/model_capture.hpp"
#include <opencv2/opencv.hpp>

using namespace cv;
using std::cout;
using std::endl;

ModelCapturer::ModelCapturer()
{
  isLoopClosed = false;

  //TODO: move up
  cameraMatrix = (Mat_<float>(3, 3) << 525.0,   0.0, 319.5,
                                         0.0, 525.0, 239.5,
                                         0.0,   0.0,   1.0);
  onlineCaptureServer.set("cameraMatrix", cameraMatrix);

  //TODO: move up
  onlineCaptureServer.set("minTranslationDiff", 0.15f);
  onlineCaptureServer.set("minRotationDiff", 15.0f);
  onlineCaptureServer.initialize(Size(640, 480));
}

void ModelCapturer::addRGBDFrame(const cv::Mat &bgrImage, const cv::Mat &depth)
{
  if (isLoopClosed)
    return;

  std::cout << "adding the frame #" << allBgrImages.size() << std::endl;

  CV_Assert(bgrImage.type() == CV_8UC3);
  CV_Assert(depth.type() == CV_32FC1);
  CV_Assert(bgrImage.size() == depth.size());

  static int frameID = 0;
  Mat currentPose = onlineCaptureServer.push(bgrImage, depth, frameID);
  ++frameID;

  //TODO: move up
  publishOdometry(currentPose, "RGBDOdometry");




  allBgrImages.push_back(bgrImage);
  allDepths.push_back(depth);





  //TODO: remove, use loop closure detection from the algorithm
  imshow("bgr view", bgrImage);
  imshow("depth view", depth);
  //TODO: move up
  int key = waitKey(30);
  if (key == 27) //escape
  {
    isLoopClosed = true;
    createModel();
  }
}

void ModelCapturer::createModel()
{
  CV_Assert(allBgrImages.size() == allDepths.size());
  CV_Assert(!allBgrImages[0].empty());

  Ptr<KeyframesData> keyframesData = onlineCaptureServer.finalize();

  cout << "Frame-to-frame odometry result" << endl;
  //TODO: move up
//  publishOdometry(keyframesData->poses, "RGBDOdometry");
  showModel(allBgrImages, keyframesData->frames, keyframesData->poses, cameraMatrix, 0.005);

  vector<Mat> refinedPosesSE3;
  refineSE3Poses(keyframesData->poses, refinedPosesSE3);

  cout << "Result of the loop closure" << endl;
  //TODO: move up
  publishOdometry(refinedPosesSE3, "LoopClosure");
  showModel(allBgrImages, keyframesData->frames, refinedPosesSE3, cameraMatrix, 0.003);

  vector<Mat> refinedPosesICPSE3;
  float pointsPart = 0.05f;
  refineRgbdICPSE3Poses(keyframesData->frames, refinedPosesSE3, cameraMatrix, pointsPart, refinedPosesICPSE3);

  cout << "Result of RgbdICP for camera poses" << endl;
  //TODO: move up
  publishOdometry(refinedPosesICPSE3, "RgbdICP");
  float modelVoxelSize = 0.003f;
  showModel(allBgrImages, keyframesData->frames, refinedPosesICPSE3, cameraMatrix, modelVoxelSize);

#if 1
  // remove table from the further refinement
  for(size_t i = 0; i < keyframesData->frames.size(); i++)
  {
      keyframesData->frames[i]->mask &= ~keyframesData->tableMasks[i];
      keyframesData->frames[i]->releasePyramids();
  }
  pointsPart = 1.f;
  modelVoxelSize = 0.001;
#endif

  vector<Mat> refinedPosesICPSE3Landmarks;
  refineICPSE3Landmarks(keyframesData->frames, refinedPosesICPSE3, cameraMatrix, refinedPosesICPSE3Landmarks);

  cout << "Result of RgbdICP for camera poses and moving the model points" << endl;
  //TODO: move up
  publishOdometry(refinedPosesICPSE3Landmarks, "RgbdICP_landmarks");
  modelVoxelSize = 0.000001;
  showModel(allBgrImages, keyframesData->frames, refinedPosesICPSE3Landmarks, cameraMatrix, modelVoxelSize);
//    showModelWithNormals(allBgrImages, keyframesData->frame, refinedPosesICPSE3Landmarks, cameraMatrix);
}
