#include "processing.h"
#include <cstdio>
using namespace daisy;
using namespace daisysp;

// --- NODE IMPLEMENTATION ---

void Processing::Node::Init(const char* n, int type)
{
    name = n;
    for(int i=0; i<MAX_PARAMS; i++) {
        params[i] = {"", 0.f, 0.f, false, -1, -1};
        map_amounts[i] = 0.0f;
    }

    if (type == 1) // "Complex" (Dummy Node)
    {
        param_count = 4;
        params[0] = {"VOL", 0.5f, 0.5f, true, 4, -1};
        params[1] = {"PAN", 0.5f, 0.5f, true, 5, -1};
        params[2] = {"REV", 0.2f, 0.2f, true, 6, -1};
        params[3] = {"DLY", 0.3f, 0.3f, true, 7, -1};
        // Children
        params[4] = {" >Att", 0.1f, 0.1f, false, -1, 0};
        params[5] = {" >Wid", 0.8f, 0.8f, false, -1, 1};
        params[6] = {" >Dec", 0.4f, 0.4f, false, -1, 2};
        params[7] = {" >Fbk", 0.5f, 0.5f, false, -1, 3};
    }
    else // "Simple" (Pre/Post)
    {
        param_count = 1; 
        params[0] = {"GAIN", 0.8f, 0.8f, false, -1, -1};
    }
}

float Processing::Node::Process(float in)
{
    float gain = params[0].effective_value;
    return in * gain;
}


// --- MAIN PROCESSING ---

void Processing::Init(Hardware &hw)
{
    nodes[0].Init("PRE", 0);
    nodes[1].Init("DUMMY", 1);
    nodes[2].Init("POST", 0);
}

void Processing::Controls(Hardware &hw)
{
    float pot_val = hw.pot.Process();
    
    hw.encoder.Debounce();
    hw.button.Debounce();

    int32_t enc_inc = hw.encoder.Increment();
    bool    clicked = hw.encoder.RisingEdge() || hw.button.RisingEdge();
    bool    held    = hw.encoder.TimeHeldMs() >= 600;

    // --- HOLD LOGIC ---
    if (held && !hold_handled)
    {
        hold_handled = true;
        
        if (current_node_idx == -1) 
        {
            node_settings = !node_settings; 
        }
        else 
        {
            Node& active_node = nodes[current_node_idx];
            if (!advanced_mode)
            {
                if(param_cursor >= 0) 
                {
                    advanced_mode = true;
                    viewing_param = param_cursor;
                    adv_cursor = 0; 
                    edit_state = false;
                }
            }
            else
            {
                if (adv_cursor == 1 && active_node.params[viewing_param].has_child)
                {
                    viewing_param = active_node.params[viewing_param].child_id;
                    adv_cursor = 0;
                    edit_state = false;
                }
                else
                {
                    bool is_child = (viewing_param >= active_node.param_count); 
                    if(is_child) {
                        viewing_param = active_node.params[viewing_param].parent_id;
                        adv_cursor = 1; 
                    } else {
                        advanced_mode = false;
                    }
                }
            }
        }
    }
    else if (!hw.encoder.Pressed())
    {
        hold_handled = false;
    }

    // --- INTERACTION ---
    if (!held) 
    {
        // 1. ROOT LEVEL
        if (current_node_idx == -1)
        {
            if(node_settings)
            {
                if(clicked) node_settings = false; 
            }
            else
            {
                if (enc_inc != 0)
                {
                    root_cursor += (enc_inc > 0 ? 1 : -1);
                    if(root_cursor < 0) root_cursor = NODE_COUNT - 1;
                    if(root_cursor >= NODE_COUNT) root_cursor = 0;
                }
                if (clicked)
                {
                    current_node_idx = root_cursor;
                    param_cursor = 0; 
                    enc_click_pending = true;
                    enc_click_time = System::GetNow();
                }
            }
        }
        // 2. NODE LEVEL
        else
        {
            Node& node = nodes[current_node_idx];

            if (advanced_mode)
            {
                if (edit_state)
                {
                    if (adv_cursor == 0) // Map
                        node.map_amounts[viewing_param] = fclamp(node.map_amounts[viewing_param] + (enc_inc * 0.05f), -1.0f, 1.0f);
                    else if (adv_cursor == 1) // Child
                    {
                        int c_idx = node.params[viewing_param].child_id;
                        node.params[c_idx].base_value = fclamp(node.params[c_idx].base_value + (enc_inc * 0.05f), 0.0f, 1.0f);
                    }
                    if (clicked) edit_state = false;
                }
                else
                {
                    if (enc_inc != 0)
                    {
                        adv_cursor += (enc_inc > 0 ? 1 : -1);
                        int max_c = node.params[viewing_param].has_child ? 1 : 0;
                        if(adv_cursor < -1) adv_cursor = -1;
                        if(adv_cursor > max_c) adv_cursor = max_c;
                    }
                    if (clicked)
                    {
                        if(adv_cursor == -1) {
                            bool is_child = (viewing_param >= node.param_count);
                            if(is_child) {
                                viewing_param = node.params[viewing_param].parent_id;
                                adv_cursor = 1;
                            } else {
                                advanced_mode = false;
                            }
                        } else {
                            edit_state = true;
                        }
                    }
                }
            }
            else
            {
                if (edit_state)
                {
                    if (enc_inc != 0)
                        node.params[param_cursor].base_value = fclamp(node.params[param_cursor].base_value + (enc_inc * 0.05f), 0.0f, 1.0f);
                    if (clicked) edit_state = false;
                }
                else
                {
                    if (enc_inc != 0)
                    {
                        param_cursor += (enc_inc > 0 ? 1 : -1);
                        if(param_cursor < -1) param_cursor = -1;
                        if(param_cursor >= node.param_count) param_cursor = node.param_count - 1;
                    }

                    if (clicked)
                    {
                        if (param_cursor == -1) current_node_idx = -1;
                        else edit_state = true;
                    }
                }
            }
        }
    }

    // --- UPDATE VALUES ---
    for(int n=0; n<NODE_COUNT; n++)
    {
        for(int i=0; i<Node::MAX_PARAMS; i++)
        {
            if(nodes[n].params[i].name[0] == '\0') continue;
            
            float mod = pot_val * nodes[n].map_amounts[i];
            nodes[n].params[i].effective_value = fclamp(nodes[n].params[i].base_value + mod, 0.0f, 1.0f);
        }
    }
}

void Processing::ProcessAudio(float &outl, float &outr, float inl, float inr)
{
    float sig_l = inl;
    float sig_r = inr;
    for(int i=0; i<NODE_COUNT; i++)
    {
        sig_l = nodes[i].Process(sig_l);
        sig_r = nodes[i].Process(sig_r);
    }
    outl = sig_l;
    outr = sig_r;
    
}