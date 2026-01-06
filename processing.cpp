#include "processing.h"
#include <cstdio>
using namespace daisy;
using namespace daisysp;

void Processing::Init(Hardware &hw)
{
    // --- BUILD THE RACK ---
    // Here we construct the specific chain
    nodes[0] = new SimpleNode("PRE");
    nodes[1] = new TestNode();       // The "Dummy" Test Node
    nodes[2] = new SimpleNode("POST");

    // Initialize all nodes
    for(int i=0; i<NODE_COUNT; i++) {
        nodes[i]->Init();
    }
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
            NodeBase* active_node = nodes[current_node_idx];
            
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
                if (adv_cursor == 1 && active_node->params[viewing_param].has_child)
                {
                    viewing_param = active_node->params[viewing_param].child_id;
                    adv_cursor = 0;
                    edit_state = false;
                }
                else
                {
                    bool is_child = (viewing_param >= active_node->param_count); 
                    if(is_child) {
                        viewing_param = active_node->params[viewing_param].parent_id;
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
            NodeBase* node = nodes[current_node_idx];

            if (advanced_mode)
            {
                if (edit_state)
                {
                    if (adv_cursor == 0) // Map
                        node->map_amounts[viewing_param] = fclamp(node->map_amounts[viewing_param] + (enc_inc * 0.05f), -1.0f, 1.0f);
                    else if (adv_cursor == 1) // Child
                    {
                        int c_idx = node->params[viewing_param].child_id;
                        node->params[c_idx].base_value = fclamp(node->params[c_idx].base_value + (enc_inc * 0.05f), 0.0f, 1.0f);
                    }
                    if (clicked) edit_state = false;
                }
                else
                {
                    if (enc_inc != 0)
                    {
                        adv_cursor += (enc_inc > 0 ? 1 : -1);
                        int max_c = node->params[viewing_param].has_child ? 1 : 0;
                        if(adv_cursor < -1) adv_cursor = -1;
                        if(adv_cursor > max_c) adv_cursor = max_c;
                    }
                    if (clicked)
                    {
                        if(adv_cursor == -1) {
                            bool is_child = (viewing_param >= node->param_count);
                            if(is_child) {
                                viewing_param = node->params[viewing_param].parent_id;
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
                        node->params[param_cursor].base_value = fclamp(node->params[param_cursor].base_value + (enc_inc * 0.05f), 0.0f, 1.0f);
                    if (clicked) edit_state = false;
                }
                else
                {
                    if (enc_inc != 0)
                    {
                        param_cursor += (enc_inc > 0 ? 1 : -1);
                        if(param_cursor < -1) param_cursor = -1;
                        if(param_cursor >= node->param_count) param_cursor = node->param_count - 1;
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

    // --- UPDATE EFFECTIVE VALUES ---
    for(int n=0; n<NODE_COUNT; n++)
    {
        for(int i=0; i<NodeBase::MAX_PARAMS; i++)
        {
            if(nodes[n]->params[i].name[0] == '\0') continue;
            
            float mod = pot_val * nodes[n]->map_amounts[i];
            nodes[n]->params[i].effective_value = fclamp(nodes[n]->params[i].base_value + mod, 0.0f, 1.0f);
        }
    }
}

void Processing::ProcessAudio(float &outl, float &outr, float inl, float inr)
{
    float sig_l = inl;
    float sig_r = inr;
    
    // Chain Processing via Polymorphism
    for(int i=0; i<NODE_COUNT; i++)
    {
        sig_l = nodes[i]->Process(sig_l);
        sig_r = nodes[i]->Process(sig_r);
    }
    
    outl = sig_l;
    outr = sig_r;
}