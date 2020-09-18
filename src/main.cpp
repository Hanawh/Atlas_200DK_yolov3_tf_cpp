/**
* Copyright 2020 Huawei Technologies Co., Ltd
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.

* File main.cpp
* Description: dvpp sample main func
*/

#include <cstdint>
#include <iostream>
#include <stdlib.h>
#include <dirent.h>

#include <time.h>

#include "object_detect.h"
#include "utils.h"

#include <opencv2/opencv.hpp>
#include "opencv2/imgcodecs/legacy/constants_c.h"
#include "opencv2/imgproc/types_c.h"
using namespace std;

namespace {
uint32_t kModelWidth = 416;
uint32_t kModelHeight = 416;
const char* kModelPath = "../model/yolov3_wh.om";
}

int main(int argc, char *argv[]) {
    //检查应用程序执行时的输入,程序执行要求输入图片目录参数
    if ((argc < 2) || (argv[1] == nullptr)) {
        ERROR_LOG("Please input: ./main <image_dir>");
        return FAILED;
    }
    //实例化目标检测对象,参数为分类模型路径,模型输入要求的宽和高
    ObjectDetect detect(kModelPath, kModelWidth, kModelHeight);
    //初始化分类推理的acl资源, 模型和内存
    Result ret = detect.Init();
    if (ret != SUCCESS) {
        ERROR_LOG("Classification Init resource failed");
        return FAILED;
    }

    //获取图片目录下所有的图片文件名
    string inputImageDir = string(argv[1]);
    vector < string > fileVec;
    Utils::GetAllFiles(inputImageDir, fileVec);
    if (fileVec.empty()) {
        ERROR_LOG("Failed to deal all empty path=%s.", inputImageDir.c_str());
        return FAILED;
    }

    //逐张图片推理
    cv::Mat bgr_img;
    uint32_t W, H;
    double avg_pre=0, avg_forward=0, avg_post=0;
    int count = 0;
    for (string imageFile : fileVec) {
        count++;
        //利用cv读取图片
        clock_t start, finish;
        double duration = 0.0;
        bgr_img = cv::imread(imageFile, cv::IMREAD_UNCHANGED);
        if(!bgr_img.data)
        {
            ERROR_LOG("No data!--Exiting the program \n");
            return FAILED;
        }
        W = bgr_img.cols;
        H = bgr_img.rows;

        //preprocess
        start = clock();
        Result ret = detect.Preprocess(bgr_img, W, H);
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC;
        avg_pre += duration;

        if (ret != SUCCESS) {
            ERROR_LOG("Read file %s failed, continue to read next", imageFile.c_str());
            continue;
        }
        //Send the preprocessed pictures to the model for inference and get the inference results
        start = clock();
        aclmdlDataset* inferenceOutput = nullptr;
        ret = detect.Inference(inferenceOutput);
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC;
        avg_forward += duration;
        if ((ret != SUCCESS) || (inferenceOutput == nullptr)) {
            ERROR_LOG("Inference model inference output data failed");
            return FAILED;
        }

        //Analyze the inference output
        start = clock();
        vector<BBox> bboxesNew = detect.Postprocess(inferenceOutput, W, H);
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC;
        avg_post += duration;
        detect.DrawBoundBoxToImage(bboxesNew, imageFile);
        detect.WriteBoundBoxToTXT(bboxesNew, imageFile);
    }
    avg_pre /= count;
    avg_forward /= count;
    avg_post /= count;
    INFO_LOG("Average preprocess cost: %.6f s", avg_pre);
    INFO_LOG("Average forward cost: %.6f s", avg_forward);
    INFO_LOG("Average postprocess cost: %.6f s", avg_post);
    INFO_LOG("Average total cost: %.6f s", avg_pre+avg_forward+avg_post);
    INFO_LOG("FPS: %.6f ", 1/(avg_pre+avg_forward+avg_post));

    INFO_LOG("Execute sample success");
    return SUCCESS;
}