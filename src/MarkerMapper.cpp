
#include "MarkerMapper.hpp"
#include "Utilities.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/highgui.hpp"

#include <cstdlib>

using namespace std;
using namespace cv;

namespace YerFace {

MarkerMapper::MarkerMapper(FrameDerivatives *myFrameDerivatives, FaceTracker *myFaceTracker, EyeTracker *myLeftEyeTracker, EyeTracker *myRightEyeTracker, float myEyelidBottomPointWeight, float myEyeLineLengthPercentage, float myFaceAspectRatio, float myPercentageOfCenterLineAboveEyeLine) {
	frameDerivatives = myFrameDerivatives;
	if(frameDerivatives == NULL) {
		throw invalid_argument("frameDerivatives cannot be NULL");
	}
	faceTracker = myFaceTracker;
	if(faceTracker == NULL) {
		throw invalid_argument("faceTracker cannot be NULL");
	}
	leftEyeTracker = myLeftEyeTracker;
	if(leftEyeTracker == NULL) {
		throw invalid_argument("leftEyeTracker cannot be NULL");
	}
	rightEyeTracker = myRightEyeTracker;
	if(rightEyeTracker == NULL) {
		throw invalid_argument("rightEyeTracker cannot be NULL");
	}
	eyelidBottomPointWeight = myEyelidBottomPointWeight;
	if(eyelidBottomPointWeight < 0.0 || eyelidBottomPointWeight > 1.0) {
		throw invalid_argument("eyelidBottomPointWeight cannot be less than 0.0 or greater than 1.0");
	}
	eyeLineLengthPercentage = myEyeLineLengthPercentage;
	if(eyeLineLengthPercentage <= 0.0) {
		throw invalid_argument("eyeLineLengthPercentage cannot be less than or equal to 0.0");
	}
	faceAspectRatio = myFaceAspectRatio;
	if(faceAspectRatio <= 0.0) {
		throw invalid_argument("faceAspectRatio cannot be less than or equal to 0.0");
	}
	percentageOfCenterLineAboveEyeLine = myPercentageOfCenterLineAboveEyeLine;
	if(percentageOfCenterLineAboveEyeLine < 0.0 || percentageOfCenterLineAboveEyeLine > 1.0) {
		throw invalid_argument("percentageOfCenterLineAboveEyeLine cannot be less than 0.0 or greater than 1.0");
	}

	eyeLineSet = false;
	eyebrowLineSet = false;
	midLineSet = false;
	smileLineSet = false;
	centerLineSet = false;
	faceTransformationBaselinePointsSet = false;
	faceTransformationSet = false;

	markerSeparator = new MarkerSeparator(frameDerivatives, faceTracker);

	markerEyelidLeftTop = new MarkerTracker(EyelidLeftTop, this, frameDerivatives, markerSeparator, leftEyeTracker);
	markerEyelidRightTop = new MarkerTracker(EyelidRightTop, this, frameDerivatives, markerSeparator, rightEyeTracker);
	markerEyelidLeftBottom = new MarkerTracker(EyelidLeftBottom, this, frameDerivatives, markerSeparator, leftEyeTracker);
	markerEyelidRightBottom = new MarkerTracker(EyelidRightBottom, this, frameDerivatives, markerSeparator, rightEyeTracker);

	markerEyebrowLeftInner = new MarkerTracker(EyebrowLeftInner, this, frameDerivatives, markerSeparator);
	markerEyebrowRightInner = new MarkerTracker(EyebrowRightInner, this, frameDerivatives, markerSeparator);
	markerEyebrowLeftMiddle = new MarkerTracker(EyebrowLeftMiddle, this, frameDerivatives, markerSeparator);
	markerEyebrowRightMiddle = new MarkerTracker(EyebrowRightMiddle, this, frameDerivatives, markerSeparator);
	markerEyebrowLeftOuter = new MarkerTracker(EyebrowLeftOuter, this, frameDerivatives, markerSeparator);
	markerEyebrowRightOuter = new MarkerTracker(EyebrowRightOuter, this, frameDerivatives, markerSeparator);

	markerCheekLeft = new MarkerTracker(CheekLeft, this, frameDerivatives, markerSeparator);
	markerCheekRight = new MarkerTracker(CheekRight, this, frameDerivatives, markerSeparator);
	
	markerJaw = new MarkerTracker(Jaw, this, frameDerivatives, markerSeparator);

	markerLipsLeftCorner = new MarkerTracker(LipsLeftCorner, this, frameDerivatives, markerSeparator);
	markerLipsRightCorner = new MarkerTracker(LipsRightCorner, this, frameDerivatives, markerSeparator);

	markerLipsLeftTop = new MarkerTracker(LipsLeftTop, this, frameDerivatives, markerSeparator);
	markerLipsRightTop = new MarkerTracker(LipsRightTop, this, frameDerivatives, markerSeparator);

	markerLipsLeftBottom = new MarkerTracker(LipsLeftBottom, this, frameDerivatives, markerSeparator);
	markerLipsRightBottom = new MarkerTracker(LipsRightBottom, this, frameDerivatives, markerSeparator);
	
	fprintf(stderr, "MarkerMapper object constructed and ready to go!\n");
}

MarkerMapper::~MarkerMapper() {
	fprintf(stderr, "MarkerMapper object destructing...\n");
	//Make a COPY of the vector, because otherwise it will change size out from under us while we are iterating.
	vector<MarkerTracker *> markerTrackersSnapshot = vector<MarkerTracker *>(*MarkerTracker::getMarkerTrackers());
	size_t markerTrackersCount = markerTrackersSnapshot.size();
	for(size_t i = 0; i < markerTrackersCount; i++) {
		if(markerTrackersSnapshot[i] != NULL) {
			delete markerTrackersSnapshot[i];
		}
	}
	delete markerSeparator;
}

void MarkerMapper::processCurrentFrame(void) {
	markerSeparator->processCurrentFrame();

	markerEyelidLeftTop->processCurrentFrame();
	markerEyelidRightTop->processCurrentFrame();
	markerEyelidLeftBottom->processCurrentFrame();
	markerEyelidRightBottom->processCurrentFrame();

	calculateEyeLine();

	markerEyebrowLeftInner->processCurrentFrame();
	markerEyebrowRightInner->processCurrentFrame();
	markerEyebrowLeftMiddle->processCurrentFrame();
	markerEyebrowRightMiddle->processCurrentFrame();
	markerEyebrowLeftOuter->processCurrentFrame();
	markerEyebrowRightOuter->processCurrentFrame();

	calculateEyebrowLine();

	markerCheekLeft->processCurrentFrame();
	markerCheekRight->processCurrentFrame();

	calculateMidLine();
	calculateCenterLine(true);

	markerJaw->processCurrentFrame();

	markerLipsLeftCorner->processCurrentFrame();
	markerLipsRightCorner->processCurrentFrame();

	markerLipsLeftTop->processCurrentFrame();
	markerLipsRightTop->processCurrentFrame();

	calculateSmileLine();
	calculateCenterLine(false);

	markerLipsLeftBottom->processCurrentFrame();
	markerLipsRightBottom->processCurrentFrame();

	calculateFaceBox();

	//calculateFaceTransformation();
}

void MarkerMapper::renderPreviewHUD(bool verbose) {
	Mat frame = frameDerivatives->getPreviewFrame();
	if(verbose) {
		markerSeparator->renderPreviewHUD(true);
	}
	vector<MarkerTracker *> *markerTrackers = MarkerTracker::getMarkerTrackers();
	size_t markerTrackersCount = (*markerTrackers).size();
	for(size_t i = 0; i < markerTrackersCount; i++) {
		(*markerTrackers)[i]->renderPreviewHUD(verbose);
	}
	if(eyeLineSet) {
		line(frame, eyeLineLeft, eyeLineRight, Scalar(0, 255, 0), 2);
		Utilities::drawX(frame, eyeLineLeft, Scalar(0, 0, 255), 10, 3);
	}
	if(eyebrowLineSet) {
		line(frame, eyebrowLineLeft, eyebrowLineRight, Scalar(0, 255, 0), 2);
	}
	if(midLineSet) {
		line(frame, midLineLeft, midLineRight, Scalar(0, 255, 0), 2);
	}
	if(smileLineSet) {
		line(frame, smileLineLeft, smileLineRight, Scalar(0, 255, 0), 2);
	}
	if(centerLineSet) {
		line(frame, centerLineTop, centerLineBottom, Scalar(0, 255, 255), 2);
	}
}

tuple<Point2d, Point2d, Point2d, bool> MarkerMapper::getEyeLine(void) {
	return std::make_tuple(eyeLineLeft, eyeLineRight, eyeLineCenter, eyeLineSet);
}

tuple<Point2d, Point2d, Point2d, bool> MarkerMapper::getEyebrowLine(void) {
	return std::make_tuple(eyebrowLineLeft, eyebrowLineRight, eyebrowLineCenter, eyebrowLineSet);
}

tuple<Point2d, Point2d, Point2d, bool> MarkerMapper::getMidLine(void) {
	return std::make_tuple(midLineLeft, midLineRight, midLineCenter, midLineSet);
}

tuple<Point2d, Point2d, Point2d, bool> MarkerMapper::getSmileLine(void) {
	return std::make_tuple(smileLineLeft, smileLineRight, smileLineCenter, smileLineSet);
}

tuple<Point2d, Point2d, double, double, bool, bool> MarkerMapper::getCenterLine(void) {
	return std::make_tuple(centerLineTop, centerLineBottom, centerLineSlope, centerLineIntercept, centerLineIsIntermediate, centerLineSet);
}

void MarkerMapper::calculateEyeLine(void) {
	eyeLineSet = false;
	if(!calculateEyeCenter(markerEyelidLeftTop, markerEyelidLeftBottom, &eyeLineLeft)) {
		return;
	}
	if(!calculateEyeCenter(markerEyelidRightTop, markerEyelidRightBottom, &eyeLineRight)) {
		return;
	}

	double originalEyeLineLength = Utilities::lineDistance(eyeLineLeft, eyeLineRight);
	double desiredEyeLineLength = originalEyeLineLength * eyeLineLengthPercentage;
	eyeLineCenter = (eyeLineLeft + eyeLineRight);
	eyeLineCenter.x = eyeLineCenter.x / 2.0;
	eyeLineCenter.y = eyeLineCenter.y / 2.0;

	eyeLineLeft = Utilities::lineAdjustDistance(eyeLineCenter, eyeLineLeft, (desiredEyeLineLength / 2.0));
	eyeLineRight = Utilities::lineAdjustDistance(eyeLineCenter, eyeLineRight, (desiredEyeLineLength / 2.0));
	Utilities::lineSlopeIntercept(eyeLineLeft, eyeLineRight, &eyeLineSlope, &eyeLineIntercept);
	eyeLineSet = true;
}

bool MarkerMapper::calculateEyeCenter(MarkerTracker *top, MarkerTracker *bottom, Point2d *center) {
	Point2d topPoint;
	bool topPointSet;
	std::tie(topPoint, topPointSet) = top->getMarkerPoint();
	if(!topPointSet) {
		return false;
	}
	Point2d bottomPoint;
	bool bottomPointSet;
	std::tie(bottomPoint, bottomPointSet) = bottom->getMarkerPoint();
	if(!bottomPointSet) {
		return false;
	}
	double bottomPointWeight = eyelidBottomPointWeight;
	bottomPoint.x = bottomPoint.x * bottomPointWeight;
	bottomPoint.y = bottomPoint.y * bottomPointWeight;
	topPoint.x = topPoint.x * (1.0 - bottomPointWeight);
	topPoint.y = topPoint.y * (1.0 - bottomPointWeight);
	*center = bottomPoint + topPoint;
	return true;
}

void MarkerMapper::calculateEyebrowLine(void) {
	eyebrowLineSet = false;
	vector<MarkerTracker *> eyebrows = {markerEyebrowLeftInner, markerEyebrowLeftMiddle, markerEyebrowLeftOuter, markerEyebrowRightInner, markerEyebrowRightMiddle, markerEyebrowRightOuter};
	vector<Point2d> points;
	double minX = -1.0, maxX = -1.0, innerLeftX = -1.0, innerRightX = -1.0;
	size_t count = eyebrows.size();
	for(size_t i = 0; i < count; i++) {
		bool pointSet;
		Point2d point;
		std::tie(point, pointSet) = eyebrows[i]->getMarkerPoint();
		if(pointSet) {
			if(minX < 0.0 || point.x < minX) {
				minX = point.x;
			}
			if(maxX < 0.0 || point.x > maxX) {
				maxX = point.x;
			}
			if(eyebrows[i]->getMarkerType().type == EyebrowLeftInner) {
				innerLeftX = point.x;
			}
			if(eyebrows[i]->getMarkerType().type == EyebrowRightInner) {
				innerRightX = point.x;
			}
			points.push_back(point);
		}
	}
	if(innerLeftX < 0 || innerRightX < 0) {
		return;
	}
	if(points.size() < 3) {
		return;
	}
	Utilities::lineBestFit(points, &eyebrowLineSlope, &eyebrowLineIntercept);
	eyebrowLineLeft.x = maxX;
	eyebrowLineLeft.y = (eyebrowLineSlope * maxX) + eyebrowLineIntercept;
	eyebrowLineRight.x = minX;
	eyebrowLineRight.y = (eyebrowLineSlope * minX) + eyebrowLineIntercept;
	eyebrowLineCenter.x = ((innerLeftX + innerRightX) / 2.0);
	eyebrowLineCenter.y = (eyebrowLineSlope * eyebrowLineCenter.x) + eyebrowLineIntercept;
	eyebrowLineSet = true;
}

void MarkerMapper::calculateMidLine(void) {
	midLineSet = false;
	bool midLineLeftSet;
	std::tie(midLineLeft, midLineLeftSet) = markerCheekLeft->getMarkerPoint();
	if(!midLineLeftSet) {
		return;
	}
	bool midLineRightSet;
	std::tie(midLineRight, midLineRightSet) = markerCheekRight->getMarkerPoint();
	if(!midLineRightSet) {
		return;
	}
	midLineCenter = (midLineLeft + midLineRight);
	midLineCenter.x = midLineCenter.x / 2.0;
	midLineCenter.y = midLineCenter.y / 2.0;
	Utilities::lineSlopeIntercept(midLineLeft, midLineRight, &midLineSlope, &midLineIntercept);
	midLineSet = true;
}

void MarkerMapper::calculateSmileLine(void) {
	smileLineSet = false;
	vector<MarkerTracker *> lipMarkers = {markerLipsLeftCorner, markerLipsLeftTop, markerLipsRightTop, markerLipsRightCorner};
	vector<Point2d> points;
	double minX = -1.0, maxX = -1.0;
	size_t count = lipMarkers.size();
	for(size_t i = 0; i < count; i++) {
		bool pointSet;
		Point2d point;
		std::tie(point, pointSet) = lipMarkers[i]->getMarkerPoint();
		if(!pointSet) {
			return;
		}
		if(minX < 0.0 || point.x < minX) {
			minX = point.x;
		}
		if(maxX < 0.0 || point.x > maxX) {
			maxX = point.x;
		}
		points.push_back(point);
	}
	Utilities::lineBestFit(points, &smileLineSlope, &smileLineIntercept);
	smileLineLeft.x = maxX;
	smileLineLeft.y = (smileLineSlope * maxX) + smileLineIntercept;
	smileLineRight.x = minX;
	smileLineRight.y = (smileLineSlope * minX) + smileLineIntercept;
	smileLineCenter.x = ((smileLineLeft.x + smileLineRight.x) / 2.0);
	smileLineCenter.y = (smileLineSlope * eyebrowLineCenter.x) + smileLineIntercept;
	smileLineSet = true;
}

void MarkerMapper::calculateCenterLine(bool intermediate) {
	centerLineSet = false;
	bool jawPointSet;
	Point2d jawPoint;
	if(!eyebrowLineSet || !eyeLineSet || !midLineSet) {
		return;
	}
	centerLineIsIntermediate = intermediate;

	vector<Point2d> points = {eyebrowLineCenter, eyeLineCenter, midLineCenter};
	if(!intermediate) {
		std::tie(jawPoint, jawPointSet) = markerJaw->getMarkerPoint();
		if(!jawPointSet || !smileLineSet) {
			return;
		}
		points.push_back(jawPoint);
		points.push_back(smileLineCenter);
	}
	Utilities::lineBestFit(points, &centerLineSlope, &centerLineIntercept);

	double boxWidth = Utilities::lineDistance(eyebrowLineLeft, eyebrowLineRight);
	double boxHeight = boxWidth / faceAspectRatio;
	double boxHeightAboveEyeLine = boxHeight * percentageOfCenterLineAboveEyeLine;

	centerLineTop.y = eyeLineCenter.y - boxHeightAboveEyeLine;
	centerLineTop.x = (centerLineTop.y - centerLineIntercept) / centerLineSlope;
	centerLineBottom.y = eyeLineCenter.y + boxHeight - boxHeightAboveEyeLine;
	centerLineBottom.x = (centerLineBottom.y - centerLineIntercept) / centerLineSlope;
	centerLineSet = true;
}

void MarkerMapper::calculateFaceBox(void) {
	vector<Point2d> anglesAlongCenter;
	if(!eyebrowLineSet || !eyeLineSet || !midLineSet || !smileLineSet) {
		return;
	}
	double eyebrowLineAngle = Utilities::radiansToDegrees(Utilities::lineAngleRadians(eyebrowLineLeft, eyebrowLineRight));
	double eyeLineAngle = Utilities::radiansToDegrees(Utilities::lineAngleRadians(eyeLineLeft, eyeLineRight));
	double midLineAngle = Utilities::radiansToDegrees(Utilities::lineAngleRadians(midLineLeft, midLineRight));
	double smileLineAngle = Utilities::radiansToDegrees(Utilities::lineAngleRadians(smileLineLeft, smileLineRight));
	anglesAlongCenter.push_back(Point2d(eyebrowLineAngle, eyebrowLineCenter.y));
	anglesAlongCenter.push_back(Point2d(eyeLineAngle, eyeLineCenter.y));
	anglesAlongCenter.push_back(Point2d(midLineAngle, midLineCenter.y));
	anglesAlongCenter.push_back(Point2d(smileLineAngle, smileLineCenter.y));

	double slope, intercept;
	Utilities::lineBestFit(anglesAlongCenter, &slope, &intercept);

	double eyebrowLineDistance = Utilities::lineDistance(eyebrowLineLeft, eyebrowLineRight);
	double boxWidthHalf = eyebrowLineDistance / 2.0;

	double faceBoxTopAngle = (centerLineTop.y - intercept) / slope;
	Point2d faceBoxTopLeft, faceBoxTopRight;

	Vec2d faceBoxTopVector = Utilities::radiansToVector(Utilities::degreesToRadians(faceBoxTopAngle));
	faceBoxTopLeft.x = centerLineTop.x + (faceBoxTopVector[0] * boxWidthHalf);
	faceBoxTopLeft.y = centerLineTop.y + (faceBoxTopVector[1] * boxWidthHalf);
	faceBoxTopRight.x = centerLineTop.x - (faceBoxTopVector[0] * boxWidthHalf);
	faceBoxTopRight.y = centerLineTop.y - (faceBoxTopVector[1] * boxWidthHalf);

	double faceBoxBottomAngle = (centerLineBottom.y - intercept) / slope;
	Point2d faceBoxBottomLeft, faceBoxBottomRight;

	Vec2d faceBoxBottomVector = Utilities::radiansToVector(Utilities::degreesToRadians(faceBoxBottomAngle));
	faceBoxBottomLeft.x = centerLineBottom.x + (faceBoxBottomVector[0] * boxWidthHalf);
	faceBoxBottomLeft.y = centerLineBottom.y + (faceBoxBottomVector[1] * boxWidthHalf);
	faceBoxBottomRight.x = centerLineBottom.x - (faceBoxBottomVector[0] * boxWidthHalf);
	faceBoxBottomRight.y = centerLineBottom.y - (faceBoxBottomVector[1] * boxWidthHalf);

	Mat prevFrame = frameDerivatives->getPreviewFrame();
	line(prevFrame, faceBoxTopLeft, faceBoxTopRight, Scalar(0, 0, 255), 2);
	line(prevFrame, faceBoxBottomLeft, faceBoxBottomRight, Scalar(0, 0, 255), 2);
}

void MarkerMapper::calculateFaceTransformation(void) {
	vector<Point2d> *points;
	if(!faceTransformationBaselinePointsSet) {
		points = &faceTransformationBaselinePoints;
	} else {
		points = &faceTransformationCurrentPoints;
	}
	points->clear();
	//vector<MarkerTracker *> markers = {markerEyelidLeftTop, markerEyelidRightTop, markerEyelidLeftBottom, markerEyelidRightBottom, markerEyebrowLeftInner, markerEyebrowLeftMiddle, markerEyebrowLeftOuter, markerEyebrowRightInner, markerEyebrowRightMiddle, markerEyebrowRightOuter, markerCheekLeft, markerCheekRight, markerJaw, markerLipsLeftCorner, markerLipsRightCorner, markerLipsLeftTop, markerLipsRightTop, markerLipsLeftBottom, markerLipsRightBottom };
	vector<MarkerTracker *> markers = {markerEyelidLeftTop, markerEyelidRightTop, markerEyelidLeftBottom, markerEyelidRightBottom, markerEyebrowLeftInner, markerEyebrowLeftOuter, markerEyebrowRightInner, markerEyebrowRightOuter, markerCheekLeft, markerCheekRight };

	size_t count = markers.size();
	for(size_t i = 0; i < count; i++) {
		bool pointSet;
		Point2d point;
		std::tie(point, pointSet) = markers[i]->getMarkerPoint();
		if(!pointSet) {
			return;
		}
		points->push_back(point);
	}
	if(!faceTransformationBaselinePointsSet) {
		faceTransformationBaselinePointsSet = true;
		return;
	}
	Mat rotation, translation;
	Mat essentialMatrix = findEssentialMat(faceTransformationBaselinePoints, faceTransformationCurrentPoints);
	int valid = recoverPose(essentialMatrix, faceTransformationBaselinePoints, faceTransformationCurrentPoints, rotation, translation);
	Vec3d rotAngles = Utilities::rotationMatrixToEulerAngles(rotation);
	fprintf(stderr, "MarkerMapper: Recovered facial transformation with %d valid points. Rotation: <%.02f, %.02f, %.02f> Translation: <%.02f, %.02f, %.02f>\n", valid, rotAngles[0], rotAngles[1], rotAngles[2], translation.at<double>(0), translation.at<double>(1), translation.at<double>(2));
}

}; //namespace YerFace
