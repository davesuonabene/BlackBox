#pragma once
#include "daisysp.h"
#include "hw.h"
#include "config.h"
#include "nodes.h" // Include the new Node definitions

struct Processing
{
    // --- STATE ---
    
    // The Chain: Array of Pointers to Generic Nodes
    static const int NODE_COUNT = 3;
    NodeBase* nodes[NODE_COUNT]; 
    
    // Navigation State
    int current_node_idx = -1; // -1 = Root, 0-2 = Node View
    
    // UI Logic
    bool     edit_state = false;    
    bool     advanced_mode = false; 
    bool     node_settings = false; 
    
    // Cursors
    int      root_cursor = 0;       
    int      param_cursor = 0;      
    int      adv_cursor = 0;        
    
    // Context
    int      viewing_param = 0;     

    // Utilities
    bool     enc_click_pending = false;
    uint32_t enc_click_time    = 0;
    bool     hold_handled = false;

    // --- FUNCTIONS ---
    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void ProcessAudio(float &outl, float &outr, float inl, float inr);
};