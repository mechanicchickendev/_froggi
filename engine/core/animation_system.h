#pragma once

#include "pond_interface.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace froggi {

///////////////////////////////////////////////////////////////////////////////
// Animation Clip - Represents a sequence of OBJ files

struct AnimationClip {
    std::string name;
    std::vector<std::string> frameNames;  // Mesh names for each frame
    float frameRate = 24.0f;              // Frames per second
    bool loop = true;                     // Should animation loop?
    
    AnimationClip() = default;
    AnimationClip(const std::string& n) : name(n) {}
};

///////////////////////////////////////////////////////////////////////////////
// Animator Component - Controls animation playback

class Animator : public Component {
public:
    Animator() = default;
    
    void onUpdate(float deltaTime) override;
    
    // Animation control
    void play(const std::string& clipName, bool forceRestart = false);
    void stop();
    void pause();
    void resume();
    void setSpeed(float speed) { playbackSpeed = speed; }
    
    // Query state
    bool isPlaying() const { return playing && !paused; }
    bool isPaused() const { return paused; }
    float getCurrentTime() const { return currentTime; }
    int getCurrentFrame() const { return currentFrame; }
    const std::string& getCurrentClip() const { return currentClipName; }
    
    // Add animation clips
    void addClip(const AnimationClip& clip);
    AnimationClip* getClip(const std::string& name);
    
private:
    void updateAnimation(float deltaTime);
    void setFrame(int frame);
    
    std::unordered_map<std::string, AnimationClip> clips;
    std::string currentClipName;
    AnimationClip* currentClip = nullptr;
    
    float currentTime = 0.0f;
    int currentFrame = 0;
    float playbackSpeed = 1.0f;
    bool playing = false;
    bool paused = false;
};

///////////////////////////////////////////////////////////////////////////////
// Animation Manager - Utility for loading animation sequences

class AnimationManager {
public:
    // Load a sequence of OBJ files
    // Example: loadSequence(game, "walk", "assets/models/fig_walk_", 1, 24, ".obj")
    //   Loads: fig_walk_001.obj, fig_walk_002.obj, ..., fig_walk_024.obj
    static AnimationClip loadSequence(
        Game* game,
        const std::string& animName,
        const std::string& basePathAndPrefix,
        int startFrame,
        int endFrame,
        const std::string& extension = ".obj",
        int padding = 3,
        float frameRate = 24.0f,
        bool loop = true
    );
    
    // Load from a list of specific files
    static AnimationClip loadFromList(
        Game* game,
        const std::string& animName,
        const std::vector<std::string>& objPaths,
        float frameRate = 24.0f,
        bool loop = true
    );
};

} // namespace froggi
