#pragma once
#include "daisysp.h"
#include <cstdio>

// --- SHARED DATA STRUCTURES ---

struct ParamNode {
    const char* name;
    float base_value;      
    float effective_value; 
    bool has_child;
    int child_id;          
    int parent_id;         
};

// --- THE CONTRACT (Abstract Base) ---

struct NodeBase {
    const char* name;
    static const int MAX_PARAMS = 8;
    
    // Standard UI Data
    ParamNode params[MAX_PARAMS];
    float map_amounts[MAX_PARAMS];
    int param_count = 0;

    // Helper to clear params
    void ClearParams() {
        for(int i=0; i<MAX_PARAMS; i++) {
            params[i] = {"", 0.f, 0.f, false, -1, -1};
            map_amounts[i] = 0.0f;
        }
        param_count = 0;
    }

    // Virtual Interface
    virtual void Init() = 0;
    virtual float Process(float in) = 0;
    virtual ~NodeBase() {}
};

// --- CONCRETE NODES (Isolated Development) ---

// 1. Simple Gain Node (Used for Pre/Post)
struct SimpleNode : public NodeBase {
    const char* node_name;
    
    SimpleNode(const char* n) : node_name(n) {}

    void Init() override {
        ClearParams();
        name = node_name;
        param_count = 1;
        // Name, Base, Eff, Child?, ChildID, ParentID
        params[0] = {"GAIN", 0.8f, 0.8f, false, -1, -1};
    }

    float Process(float in) override {
        return in * params[0].effective_value;
    }
};

// 2. Test Node (The "RC-20" Style Module Dummy)
struct TestNode : public NodeBase {
    
    void Init() override {
        ClearParams();
        name = "TEST NODE";
        param_count = 4; // 4 Main controls

        // Define Main Params
        params[0] = {"VOL", 0.5f, 0.5f, true, 4, -1};
        params[1] = {"PAN", 0.5f, 0.5f, true, 5, -1};
        params[2] = {"REV", 0.2f, 0.2f, true, 6, -1};
        params[3] = {"DLY", 0.3f, 0.3f, true, 7, -1};

        // Define Children (Advanced Menu)
        params[4] = {" >Att", 0.1f, 0.1f, false, -1, 0};
        params[5] = {" >Wid", 0.8f, 0.8f, false, -1, 1};
        params[6] = {" >Dec", 0.4f, 0.4f, false, -1, 2};
        params[7] = {" >Fbk", 0.5f, 0.5f, false, -1, 3};
    }

    float Process(float in) override {
        // DSP Logic utilizing parameters
        // Example: Simple volume + Pan simulation (mono sum)
        float vol = params[0].effective_value;
        return in * vol;
    }
};