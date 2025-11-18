#include "animation_system.h"
#include "pond_interface.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace froggi {

///////////////////////////////////////////////////////////////////////////////
// Animator Component Implementation

void Animator::onUpdate(float deltaTime) {
    if (playing && !paused && currentClip) {
        updateAnimation(deltaTime);
    }
}

void Animator::play(const std::string& clipName, bool forceRestart) {
   // std::cout << "[Animator] Attempting to play: " << clipName << std::endl;
    //std::cout << "[Animator] Available clips: " << clips.size() << std::endl;
    
    auto it = clips.find(clipName);
    if (it == clips.end()) {
        std::cerr << "[Animator] ERROR: Animation clip not found: " << clipName << std::endl;
        std::cerr << "[Animator] Available clips are: ";
        for (const auto& pair : clips) {
            std::cerr << pair.first << " ";
        }
        std::cerr << std::endl;
        return;
    }
    
    // If already playing this clip and not forcing restart, continue
    if (currentClipName == clipName && playing && !forceRestart) {
        paused = false;
        return;
    }
    
    currentClipName = clipName;
    currentClip = &it->second;
    currentTime = 0.0f;
    currentFrame = 0;
    playing = true;
    paused = false;
    
   // std::cout << "[Animator] Starting clip: " << clipName 
         //     << " with " << currentClip->frameNames.size() << " frames at " 
        //      << currentClip->frameRate << " fps" << std::endl;
    
    setFrame(0);
}

void Animator::stop() {
    playing = false;
    paused = false;
    currentTime = 0.0f;
    currentFrame = 0;
}

void Animator::pause() {
    paused = true;
}

void Animator::resume() {
    paused = false;
}

void Animator::addClip(const AnimationClip& clip) {
    clips[clip.name] = clip;
}

AnimationClip* Animator::getClip(const std::string& name) {
    auto it = clips.find(name);
    if (it != clips.end()) {
        return &it->second;
    }
    return nullptr;
}

void Animator::updateAnimation(float deltaTime) {
    if (!currentClip || currentClip->frameNames.empty()) return;
    
    // Update time
    currentTime += deltaTime * playbackSpeed;
    
    // Calculate frame duration
    float frameDuration = 1.0f / currentClip->frameRate;
    
    // Calculate new frame
    int totalFrames = static_cast<int>(currentClip->frameNames.size());
    int newFrame = static_cast<int>(currentTime / frameDuration);
    
    // Handle looping
    if (newFrame >= totalFrames) {
        if (currentClip->loop) {
            currentTime = 0.0f;
            newFrame = 0;
       //     std::cout << "[Animator] Looping animation: " << currentClipName << std::endl;
        } else {
            // Animation finished
            newFrame = totalFrames - 1;
            playing = false;
        //    std::cout << "[Animator] Animation finished: " << currentClipName << std::endl;
        }
    }
    
    // Update frame if changed
    if (newFrame != currentFrame) {
       // std::cout << "[Animator] Frame change: " << currentFrame << " -> " << newFrame 
          //        << " (time: " << currentTime << "s)" << std::endl;
        currentFrame = newFrame;
        setFrame(currentFrame);
    }
}

void Animator::setFrame(int frame) {
    if (!owner || !currentClip) {
        std::cerr << "[Animator] ERROR: Cannot set frame - owner or currentClip is null" << std::endl;
        return;
    }
    
    if (frame < 0 || frame >= static_cast<int>(currentClip->frameNames.size())) {
        std::cerr << "[Animator] ERROR: Frame out of bounds: " << frame << std::endl;
        return;
    }
    
    // Get mesh component
    MeshComponent* meshComp = owner->getComponent<MeshComponent>();
    if (!meshComp) {
        std::cerr << "[Animator] ERROR: Animator requires MeshComponent on owner" << std::endl;
        return;
    }
    
    // Update mesh to current frame
    std::string newMeshName = currentClip->frameNames[frame];
  //  std::cout << "[Animator] Setting mesh to: " << newMeshName << std::endl;
    meshComp->meshName = newMeshName;
}

///////////////////////////////////////////////////////////////////////////////
// Animation Manager Implementation

AnimationClip AnimationManager::loadSequence(
    Game* game,
    const std::string& animName,
    const std::string& basePathAndPrefix,
    int startFrame,
    int endFrame,
    const std::string& extension,
    int padding,
    float frameRate,
    bool loop)
{
  //  std::cout << "[AnimationManager] Loading sequence: " << animName << std::endl;
   // std::cout << "[AnimationManager] Path: " << basePathAndPrefix << "[" << startFrame << "-" << endFrame << "]" << extension << std::endl;
    
    AnimationClip clip(animName);
    clip.frameRate = frameRate;
    clip.loop = loop;
    
    for (int i = startFrame; i <= endFrame; i++) {
        // Generate padded frame number (e.g., 001, 002, etc.)
        std::ostringstream frameNum;
        frameNum << std::setw(padding) << std::setfill('0') << i;
        
        // Build full path
        std::string fullPath = basePathAndPrefix + frameNum.str() + extension;
        
        // Generate mesh name (remove path and extension)
        size_t lastSlash = fullPath.find_last_of("/\\");
        size_t lastDot = fullPath.find_last_of(".");
        std::string meshName = fullPath.substr(
            lastSlash == std::string::npos ? 0 : lastSlash + 1,
            lastDot == std::string::npos ? std::string::npos : lastDot - (lastSlash == std::string::npos ? 0 : lastSlash + 1)
        );
        
       // std::cout << "[AnimationManager] Loading frame " << i << ": " << fullPath << " -> " << meshName << std::endl;
        
        // Load the model
        game->loadModel(meshName, fullPath);
        
        // Add to clip
        clip.frameNames.push_back(meshName);
    }
    
  //  std::cout << "[AnimationManager] Loaded animation '" << animName << "' with " 
            //  << clip.frameNames.size() << " frames at " 
           //   << frameRate << " fps" << std::endl;
    
    return clip;
}

AnimationClip AnimationManager::loadFromList(
    Game* game,
    const std::string& animName,
    const std::vector<std::string>& objPaths,
    float frameRate,
    bool loop)
{
    AnimationClip clip(animName);
    clip.frameRate = frameRate;
    clip.loop = loop;
    
    for (const auto& path : objPaths) {
        // Generate mesh name from path
        size_t lastSlash = path.find_last_of("/\\");
        size_t lastDot = path.find_last_of(".");
        std::string meshName = path.substr(
            lastSlash == std::string::npos ? 0 : lastSlash + 1,
            lastDot == std::string::npos ? std::string::npos : lastDot - (lastSlash == std::string::npos ? 0 : lastSlash + 1)
        );
        
        // Load the model
        game->loadModel(meshName, path);
        
        // Add to clip
        clip.frameNames.push_back(meshName);
    }
    
   // std::cout << "Loaded animation '" << animName << "' with " 
          //    << clip.frameNames.size() << " frames at " 
           //   << frameRate << " fps" << std::endl;
    
    return clip;
}

} // namespace froggi
