#pragma once

#include <vector>
#include <memory>
#include <string>

class ONNXInterface
{
public:
    ONNXInterface();
    ~ONNXInterface();
    
    bool loadModel(const std::string& modelPath);
    std::vector<float> runInference(const std::vector<float>& inputData);
    
    bool isModelLoaded() const { return modelLoaded; }
    
private:
    bool modelLoaded = false;
    
    // Future: ONNX Runtime session will be stored here
    // std::unique_ptr<Ort::Session> session;
    // std::unique_ptr<Ort::Env> env;
    
    // For now, simulate inference
    std::vector<float> simulateInference(const std::vector<float>& inputData);
};