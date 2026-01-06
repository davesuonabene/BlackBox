#pragma once
#include "daisysp.h"
#include "hw.h"
#include "config.h"

struct Processing
{
    // --- DATA STRUCTURES ---
    
    // Parameter Definition
    struct ParamNode {
        const char* name;
        float base_value;      
        float effective_value; 
        bool has_child;
        int child_id;          
        int parent_id;         
    };

    // A Processing Node (Effect/Module)
    struct Node {
        const char* name;
        static const int MAX_PARAMS = 8;
        ParamNode params[MAX_PARAMS];
        float map_amounts[MAX_PARAMS];
        int param_count;

        void Init(const char* name, int type); // 0=Simple, 1=Complex
        float Process(float in);
    };

    // --- STATE ---
    
    // Nodes
    static const int NODE_COUNT = 3;
    Node nodes[NODE_COUNT]; // PRE, DUMMY, POST
    
    // Navigation State
    int current_node_idx = -1; // -1 = Root (Chain View), 0-2 = Node View
    
    // UI Logic
    bool     edit_state = false;    // Editing Value or Map Amount
    bool     advanced_mode = false; // Parameter Advanced Menu (Tree/Map)
    bool     node_settings = false; // Node Special Settings (from Root Hold)
    
    // Cursors
    int      root_cursor = 0;       // Selecting Node in Root
    int      param_cursor = 0;      // Selecting Param in Node View
    int      adv_cursor = 0;        // Selecting Item in Advanced Menu
    
    // Context
    int      viewing_param = 0;     // Current param index in Advanced Mode

    // Utilities
    bool     enc_click_pending = false;
    uint32_t enc_click_time    = 0;
    bool     hold_handled = false;

    // --- FUNCTIONS ---
    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void ProcessAudio(float &outl, float &outr, float inl, float inr);
};