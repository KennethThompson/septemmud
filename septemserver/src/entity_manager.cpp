#include "entity_manager.h"
#include "script_entities/script_entity.h"
#include "script_entities/roomobj.h"
#include "script_entities/livingentity.h"
#include "script_entities/playerobj.h"
#include "script_entities/daemonobj.h"
#include "script_entities/commandobj.h"
#include "string_utils.h"
#include "global_settings.h"
#include "config.h"
#include <plog/Log.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>
#include <unordered_set>


namespace fs = boost::filesystem;

void get_entity_str(EntityType etype, std::string& str)
{
   
    switch(etype) {
    case EntityType::DAEMON: {
        str = "DAEMON";
    } break;
    case EntityType::COMMAND: {
        str = "COMMAND";
    } break;
    case EntityType::ITEM: {
        str = "ITEM";
    } break;
    case EntityType::NPC: {
        str = "NPC";
    } break;
    case EntityType::PLAYER: {
        str = "PLAYER";
    } break;
    case EntityType::ROOM: {
        str = "ROOM";
    } break;
    default:
        str = "UNKNOWN";
        break;
    }

}

bool entity_manager::compile_script(std::string& file_path, std::string& reason)
{
    LOG_DEBUG << "Compiling script: " << file_path;
    
    fs::path p(file_path);
    if(!fs::exists(p)) {
        reason = "Script does not exist.";
        return false;
    }
    
    std::string script_ = std::regex_replace(file_path, std::regex("\\" + 
            global_settings::Instance().GetSetting(DEFAULT_GAME_DATA_PATH)), "");  
    _currently_loading_script = script_;
    
    if(!m_state) {
        m_state = std::shared_ptr<sol::state>(new sol::state);
        init_lua();
    }
    //lua_safe_script(path, *m_state);
    std::string script_text;
    EntityType etype;
    std::string error_str;
    

    if(!load_script_text(file_path, script_text, etype, error_str)) {
        reason = error_str;
        return false;
    }
    /*
    for( int x = 2; x < 1000; x++ )
    {
        
        script_text += "p"+std::to_string(x) + " = room.new()\r\n";
        script_text += "p"+std::to_string(x) + ":SetTitle('" + "ROOM" + std::to_string(x) + "')\r\n";
        script_text += "function p" + std::to_string(x) +":test() print(p"+ std::to_string(x) +":GetTitle()) end\r\n";
        script_text += "y = register_heartbeat(p"+std::to_string(x)+".test)\r\n";
        
            
        script_text += "\r\n";
    }
    */
    //std::cout << script_text;
    
    
    // First step get the correct collcetion..
  //  std::unordered_map< std::string, entity_wrapper > & ent_ = m_room_objs; // just to initialize it
    switch( etype )
    {
        case EntityType::ROOM:
        {
            // Check for existing rooms
            std::set<std::shared_ptr<entity_wrapper>> rooms;
            get_rooms_from_path(script_, rooms);
            
            for( auto& r : rooms )
            {
               // script_entity& self = r->script_ent.value().selfobj.as<script_entity>();
                //*self = NULL;
                //r->script_ent.value().selfobj = NULL;
                //r->script_env.value() = sol::nil;
                //(*r->script_state).collect_garbage();
            }
            if( rooms.size() > 0 )
                destroy_room(script_);
            

        }
        break;
        case EntityType::PLAYER:
        {
            // TODO: logic that finds the player and reconnects them with their client object, if they have an object in the world
        }
        break;
        case EntityType::DAEMON:
        {
            // TODO:  logic to register daemon so scripts can find it..
            
        }
        break;
        default:
        break;
    }
    sol::environment to_load;
    _init_entity_env(script_, etype, to_load);
    
    std::string test_ = to_load["_INTERNAL_SCRIPT_PATH_"];
    assert(  test_ == script_ );
   
    lua_safe_script(script_text, to_load, reason);
    _currently_loading_script = "";
    return true;
}

bool entity_manager::load_player()
{
    std::string player_script_path = global_settings::Instance().GetSetting(DEFAULT_GAME_DATA_PATH);
    player_script_path += global_settings::Instance().GetSetting(BASE_PLAYER_ENTITY_PATH);
    std::string reason;
    try
    {
        if( !fs::exists(player_script_path) )
        {
            reason = "Unable to locate base player script.";
            return false;
        }
    
    // this may need to be rethought.  But for now, create a temporary script file..
        std::string tmp_p = player_script_path + "_caliel"; // <-- static for now
        if( fs::exists(tmp_p) )
        {
            fs::remove(tmp_p);
        }
        fs::copy(player_script_path, tmp_p);
        
        if(!compile_script(tmp_p, reason)) 
        {
            fs::remove(tmp_p);
            return false;
        }
        fs::remove(tmp_p);
    }
    catch( std::exception& ex )
    {
        reason = ex.what();
        return false;
    }

    //fs::copy()

    /*
    std::string ret;
    if(compile_script(fpath, entities, reason)) {

        for(auto ent : entities) {

            switch(ent->entity_type) {
            case EntityType::PLAYER: {
                if(ent->script_obj) {
                    base_entity* be = &ent->script_obj.value();
                    player_entity* pe = dynamic_cast<player_entity*>(be);
                    pe->client_obj = c;
                    pe->player_name = c->get_account()->username;
                    LOG_INFO << "Successfully loaded player: " << pe->player_name;
                } else {
                    LOG_ERROR << "Unable to cast player object into player_entity.";
                }

                player_objs.insert({ ent->get_object_uid(), ent });

                entity_wrapper& ew = *void_room;
                move_entity(ent, ew);
                ret = ent->get_object_uid();
            } break;
            default:
                break;
            }
        }

        
    } else {
        LOG_DEBUG << "Error. Unable to load player: " << reason;
       // return "";
    }
     */
    return true;
}

bool entity_manager::lua_safe_script(std::string& script_text, sol::environment env, std::string& reason)
{
    sol::state & lua = (*m_state);
    /*
    auto simple_handler = [](lua_State*, sol::protected_function_result result) {
        // You can just pass it through to let the call-site handle it
        return result;
    };
    auto result = lua.script(script_text, env, simple_handler);
    if(result.valid()) {
        LOG_DEBUG << "Successfully executed script.";
        return true;
    } else {
        sol::error err = result;
        LOG_ERROR << err.what();
    }
    */
    sol::load_result lr = lua.load(script_text);
    if(!lr.valid()) {
        sol::error err = lr;
        reason = err.what();
        return false;
    }
    
    _current_script_f_ = lr;//lua.load(script_text);
    
    env.set_on(_current_script_f_);
    
    sol::protected_function_result result = _current_script_f_(); 
    if(!result.valid()) {
        sol::error err = result;
        reason = err.what();
        // std::cout << "call failed, sol::error::what() is " << what << std::endl;
        return false;
    } 
  //  _current_script_f_ = pf;
   // sol::environment genv = lua.globals();
   // genv.set_on(pf);

    return true;
}

template <typename T> T* downcast(script_entity* b)
{
    return dynamic_cast<T*>(b);
}


void entity_manager::init_lua()
{
    sol::state & lua = (*m_state);
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table);
    lua.new_usertype<roomobj>("room",
      sol::constructors<roomobj(sol::this_state)>(),
      sol::meta_function::new_index, &roomobj::set_property_lua,
      sol::meta_function::index, &roomobj::get_property_lua,
      "GetTitle", &roomobj::GetTitle,
      "SetTitle", &roomobj::SetTitle,
      "GetDescription", &roomobj::GetDescription,
      "SetDescription", &roomobj::SetDescription,
      "GetShortDescription", &roomobj::GetShortDescription,
      "SetShortDescription", &roomobj::SetShortDescription,
      "GetExits", &roomobj::GetExits,
      "AddExit", &roomobj::AddExit,
      "GetType", &script_entity::GetType,
      sol::base_classes,
      sol::bases<script_entity>());
      
    
    lua.new_usertype<daemonobj>("daemon",
      sol::constructors<daemonobj(sol::this_state ts, std::string)>(),
      sol::meta_function::new_index, &daemonobj::set_property_lua,
      sol::meta_function::index, &daemonobj::get_property_lua,
      "GetName", &daemonobj::GetName,
      "SetName", &daemonobj::SetName,
      "GetType", &script_entity::GetType,
      sol::base_classes,
      sol::bases<script_entity>());
      
    lua.new_usertype<playerobj>("player",
      sol::constructors<playerobj(sol::this_state)>(),
      sol::meta_function::new_index, &playerobj::set_property_lua,
      sol::meta_function::index, &playerobj::get_property_lua,
        "player_name", &playerobj::player_name,
        "SendToEntity", &playerobj::SendToEntity,
        "GetEnvironment", &playerobj::GetEnvironment,
        "SetEnvironment", &playerobj::SetEnvironment,
        "GetRoom", &playerobj::GetRoom,
        "GetType", &script_entity::GetType,
        sol::base_classes,
        sol::bases<living_entity, script_entity>());
        
        
    lua.new_usertype<commandobj>("command",
        sol::constructors<commandobj(sol::this_state)>(),
        "GetName", &commandobj::GetName,
        "SetName", &commandobj::SetName,
        "GetSynonyms", &commandobj::GetSynonyms,
        "SetSynonyms", &commandobj::SetSynonyms,
        "SetPriority", &commandobj::SetPriority,
        "GetPriority", &commandobj::GetPriority,
        sol::base_classes,
        sol::bases<script_entity>());

    
    
    lua.set_function("register_heartbeat", &heartbeat_manager::register_heartbeat_func_on, &_heartbeat);
    lua.set_function("deregister_heartbeat", &heartbeat_manager::deregister_heartbeat_func, &_heartbeat);
    lua.set_function("deregister_all_heartbeat", &heartbeat_manager::deregister_all_heartbeat_funcs, &_heartbeat);
    
    lua.set_function("command_cast", &downcast<commandobj>);
    lua.set_function("room_cast", &downcast<roomobj>);
    lua.set_function("player_cast", &downcast<playerobj>);
    
    lua.set_function("get_base_commands", [&]() -> std::map<std::string, commandobj*>& {
		return m_base_cmds;
	});
    
   
/*
    sol::environment global_env = lua.globals();
    sol::environment base_env;

    _init_lua_env_(lua, global_env, global_env, "base_entity_", base_env);

    sol::environment daemon_env;
    _init_lua_env_(lua, global_env, base_env, get_env_str(EntityType::DAEMON), daemon_env);

    sol::environment command_env;
    _init_lua_env_(lua, global_env, base_env, get_env_str(EntityType::COMMAND), command_env);

    sol::environment item_env;
    _init_lua_env_(lua, global_env, base_env, get_env_str(EntityType::ITEM), item_env);

    sol::environment npc_env;
    _init_lua_env_(lua, global_env, base_env, get_env_str(EntityType::NPC), npc_env);

    sol::environment player_env;
    _init_lua_env_(lua, global_env, base_env, get_env_str(EntityType::PLAYER), player_env);

    sol::environment room_env;
    _init_lua_env_(lua, global_env, base_env, get_env_str(EntityType::ROOM), room_env);
*/
}


bool entity_manager::load_script_text(std::string& script_path,
                                      std::string& script_text,
                                      EntityType& obj_type,
                                      std::string& reason)
{
    std::ifstream in(script_path);
    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string token;
    std::vector<std::string> file_tokens;

    bool bFoundType = false;
    while(std::getline(buffer, token, '\n')) {
        trim(token);
        if(token.compare("inherit daemon") == 0) {
            if(bFoundType) // bad. only one type per script
            {
                reason = "Multiple inherit directives detected. Only one entity type is allowed per script.";
                return false;
            }
            obj_type = EntityType::DAEMON;
            token = "";
            file_tokens.push_back(token);

            bFoundType = true;
        } else if(token.compare("inherit room") == 0) {
            if(bFoundType) // bad. only one type per script
            {
                reason = "Multiple inherit directives detected. Only one entity type is allowed per script.";
                return false;
            }
            obj_type = EntityType::ROOM;
            token = "";
            file_tokens.push_back(token);

            bFoundType = true;
        } else if(token.compare("inherit player") == 0) {
            if(bFoundType) // bad. only one type per script
            {
                reason = "Multiple inherit directives detected. Only one entity type is allowed per script.";
                return false;
            }
            obj_type = EntityType::PLAYER;
            token = "";
            file_tokens.push_back(token);
            bFoundType = true;
        } else if(token.compare("inherit object") == 0) {
            if(bFoundType) // bad. only one type per script
            {
                reason = "Multiple inherit directives detected. Only one entity type is allowed per script.";
                return false;
            }
            obj_type = EntityType::ITEM;
            token = "";
            file_tokens.push_back(token);
            bFoundType = true;
        } else if(token.compare("inherit npc") == 0) {
            if(bFoundType) // bad. only one type per script
            {
                reason = "Multiple inherit directives detected. Only one entity type is allowed per script.";
                return false;
            }
            obj_type = EntityType::NPC;
            token = "";
            file_tokens.push_back(token);

            bFoundType = true;
        } else if(token.compare("inherit command") == 0) {
            if(bFoundType) // bad. only one type per script
            {
                reason = "Multiple inherit directives detected. Only one entity type is allowed per script.";
                return false;
            }
            obj_type = EntityType::COMMAND;
            token = "";
            file_tokens.push_back(token);

            bFoundType = true;
        } else {
            file_tokens.push_back(token);
        }
    }
    script_text = vector_to_string(file_tokens, '\n');
    if(bFoundType == false) {
        reason = "No inherit directive found in script.";
        return false;
    }
    return true;
}
/*
bool entity_manager::get_room_by_script(std::string& script_path, std::unordered_set<entity_wrapper>& room_ents)
{
    auto search = m_room_objs.find(script_path);
    if(search == m_room_objs.end()) {
        return false;
    }
    room_ents = search->second;
    return true;
}
*/

bool entity_manager::destroy_player(std::string& script_path)
{
    sol::state & lua = (*m_state);

    {
        auto search = m_player_objs.find(script_path);
        if(search == m_player_objs.end()) {
            return false;
        }
    
        sol::environment env;
        std::string name;
        get_parent_env_of_entity(script_path, env, name);
        
        destroy_entity(search->second);
        
        env[name] = sol::nil;
        m_player_objs.erase(search);

    }

    lua.collect_garbage();
    
    return true;
}

bool entity_manager::destroy_room(std::string& script_path)
{

    sol::state & lua = (*m_state);
    {
        auto search = m_room_objs.find(script_path);
        if(search == m_room_objs.end()) {
            return false;
        }
    
        sol::environment env;
        std::string name;
        get_parent_env_of_entity(script_path, env, name);
        
        _heartbeat.clear_heartbeat_funcs(script_path);
        
        for( auto e : search->second )
        {
            destroy_entity(e);
        }
        
        env[name] = sol::nil;
        m_room_objs.erase(search);

    }

    lua.collect_garbage();
    
    return true;
}

bool entity_manager::destroy_command(std::string& script_path)
{

    sol::state & lua = (*m_state);
    {
        auto search = m_cmd_objs.find(script_path);
        if(search == m_cmd_objs.end()) {
            return false;
        }
    
        sol::environment env;
        std::string name;
        get_parent_env_of_entity(script_path, env, name);
        
       
        for( auto e : search->second )
        {
            destroy_entity(e);
        }
        
        env[name] = sol::nil;
        m_cmd_objs.erase(search);

    }

    lua.collect_garbage();
    
    return true;
}

bool entity_manager::destroy_daemon(std::string& script_path)
{

    sol::state & lua = (*m_state);
    {
        auto search = m_daemon_objs.find(script_path);
        if(search == m_daemon_objs.end()) {
            return false;
        }
    
        sol::environment env;
        std::string name;
        get_parent_env_of_entity(script_path, env, name);
        
        for( auto e : search->second )
        {
            destroy_entity(e);
        }
        sol::environment en_ = (*m_state).globals();//env[name];
        env[name] = sol::nil;
        m_daemon_objs.erase(search);

    }

    lua.collect_garbage();
    
    return true;
}


bool entity_manager::destroy_entity(std::shared_ptr<entity_wrapper>& ew)
{
    ew->script_ent->clear_props();// = NULL;
    sol::environment genv = (*m_state).globals();
    genv.set_on(ew->_script_f_);
    ew.reset();
    return true;
}


bool entity_manager::_init_entity_env( std::string& script_path, EntityType etype, sol::environment& env )
{
    //std::string envstr = std::regex_replace(script_path, std::regex("\\" + 
    //global_settings::Instance().GetSetting(DEFAULT_GAME_DATA_PATH)), "");  // remove the extra pathing stuff we don't need
        
    sol::state & lua = (*m_state);
    
    std::vector<std::string> strs;
    boost::split(strs, script_path, boost::is_any_of("/"));
    
    sol::environment current_env = lua.globals();
    
    for( auto& en : strs )
    {
        sol::optional<sol::environment> maybe_env = current_env[en+"_env_"];
        LOG_DEBUG << "Looking for environment: " << en << "_env_";
        if( maybe_env )
        {
            // it has already been initialized
            current_env = maybe_env.value(); // set it so we can go deeper..
            LOG_DEBUG << "Environment already exists: " << en << "_env_";
            //LOG_VERBOSE << "Environment [" << en << "] already exists.";
        }
        else
        {
            // be careful with the next call, if the environment exists then
            // this function will destroy it.
            LOG_DEBUG << "Initializing environment: " << en << "_env_";
            _init_lua_env_(current_env, current_env, en+"_env_", current_env);
           // just for now, so we can sanity check where we are..
           current_env["_FS_PATH_"] = en+"_env_";
        }
    }
    
    current_env["_INTERNAL_SCRIPT_PATH_"] = script_path;
    std::string s;
    get_entity_str(etype, s);
    current_env["_INTERNAL_ENTITY_TYPE_"] = s;
    env = current_env;
    return true;
}

bool entity_manager::_init_lua_env_(sol::environment parent,
                                    sol::environment inherit,
                                    std::string new_child_env_name,
                                    sol::environment& new_child_env)
{
    sol::state & lua = (*m_state);
    sol::optional<sol::environment> test_env = parent[new_child_env_name]; // parent.get<sol::environment>( new_child_env_name );
    if(test_env) {
        LOG_DEBUG << "Destroying environment: " << new_child_env_name;
        parent[new_child_env_name] = sol::nil;
        lua.collect_garbage();
    }
    sol::environment tmp = sol::environment(lua, sol::create, inherit);
    parent[new_child_env_name] = tmp;
    //std::string force_weak = "setmetatable(" + new_child_env_name + ", {__mode = 'k'})"; 
    //lua.script( force_weak , parent);
   // __index = _G
    //tmp["__index"] = "_G"; 
    new_child_env = tmp;
    LOG_DEBUG << "Creating environment: " << new_child_env_name;
    return true;
}

void entity_manager::invoke_heartbeat()
{
    _heartbeat.do_heartbeats();
}

void entity_manager::register_entity(script_entity& entityobj, EntityType etype)
{
    std::shared_ptr<entity_wrapper> ew( new entity_wrapper );
    ew->entity_type = etype;
    ew->script_path = GetCurrentlyCompiledScript();
    entityobj.SetScriptPath(ew->script_path);
    ew->script_ent = &entityobj;
    ew->_script_f_ = _current_script_f_; // <-- work around to ensure all objects in env get destroyed
            
            
            
    sol::environment e_parent;
    std::string env_name;
    get_parent_env_of_entity(ew->script_path, e_parent, env_name);
    
    //  The idea behind the next few calls is to make sure that each entity gets it's own
    //  instance ID.  Each time a entity loads we look to the parent env to grab 
    //  the intance ID, then we increment for the next entity.  Most of the time
    //  only 1 entity will be instantiated per script, so this will only be used
    //  when people get clever.  Primarily, this allows the entity manager
    //  to figure out which object is being referenced, e.g., during a destroy, so
    //  the other entities declared in the script remain untouched
    sol::optional<int> instance_id = e_parent[ env_name ]["_internal_instance_id_"];
    if( !instance_id )
    {
        e_parent[ env_name ]["_internal_instance_id_"] = 1;
        ew->instance_id = 0;
    }
    else
    {
        ew->instance_id = instance_id.value();
        int new_id = instance_id.value()+1;
        e_parent[ env_name ]["_internal_instance_id_"] = new_id;
    }
    
    switch( etype )
    {
        case EntityType::PLAYER:
        {

            auto search = m_player_objs.find(GetCurrentlyCompiledScript());
            if(search != m_player_objs.end()) 
            {

                //search->second.insert(ew);
            }
            else
            {
                m_player_objs.insert( {ew->script_path, ew} );// new_set ); //new_set );get_entity_str() 
                std::string e_str;
                get_entity_str(ew->entity_type, e_str); 
                LOG_DEBUG << "Registered new entity, type = " << e_str << ", Path =" << ew->script_path;
            }
        }
        break;
        case EntityType::ROOM:
        {           
            // get the current instance ID from the env..
            auto search = m_room_objs.find(GetCurrentlyCompiledScript());
            if(search != m_room_objs.end()) {

                search->second.insert(ew);
            }
            else
            {
                std::set<std::shared_ptr<entity_wrapper>> new_set;
                new_set.insert( ew );
                m_room_objs.insert( {ew->script_path, new_set} );// new_set ); //new_set );get_entity_str() 
                std::string e_str;
                get_entity_str(ew->entity_type, e_str); 
                LOG_DEBUG << "Registered new entity, type = " << e_str << ", Path =" << ew->script_path;
            }
        }
        break;
        case EntityType::COMMAND:
        {           
            // get the current instance ID from the env..
            commandobj * cmdobj = dynamic_cast<commandobj*>(&entityobj);
            std::string cmd_noun = cmdobj->GetName();
            auto cmd_search = m_base_cmds.find(cmd_noun);
            /*
            if( cmd_search != m_base_cmds.end() )
            {
                m_base_cmds.erase(cmd_search); // may need to move this destruct code elsewhere
            }
            */
            m_base_cmds[cmd_noun] = cmdobj;
            
            auto search = m_cmd_objs.find(GetCurrentlyCompiledScript());
            if(search != m_cmd_objs.end()) {

                search->second.insert(ew);
            }
            else
            {
                std::set<std::shared_ptr<entity_wrapper>> new_set;
                new_set.insert( ew );
                m_cmd_objs.insert( {ew->script_path, new_set} );// new_set ); //new_set );get_entity_str() 
                std::string e_str;
                get_entity_str(ew->entity_type, e_str); 
                LOG_DEBUG << "Registered new entity, type = " << e_str << ", Path =" << ew->script_path;
                
                
            }
        }
        break;
        case EntityType::DAEMON:
        {
            auto search = m_daemon_objs.find(GetCurrentlyCompiledScript());
            if(search != m_daemon_objs.end()) {

                search->second.insert(ew);
            }
            else
            {
                std::set<std::shared_ptr<entity_wrapper>> new_set;
                new_set.insert( ew );
                m_daemon_objs.insert( {ew->script_path, new_set} );// new_set ); //new_set );get_entity_str() 
                std::string e_str;
                get_entity_str(ew->entity_type, e_str); 
                LOG_DEBUG << "Registered new entity, type = " << e_str << ", Path =" << ew->script_path;
            }
        }
        break;
        default:
        break;
    }
}

void entity_manager::deregister_entity(script_entity& entityobj, EntityType etype)
{
    return; // just for now
    /*
    switch( etype )
    {
        case EntityType::ROOM:
        {
            auto search = m_room_objs.find(entityobj.GetScriptPath());
            if(search != m_room_objs.end()) 
            {
                m_room_objs.erase(search);
            }
        }
        break;
        case EntityType::PLAYER:
        {
            auto search = m_player_objs.find(entityobj.GetScriptPath());
            if(search != m_player_objs.end()) 
            {
                m_player_objs.erase(search);
            }
        }
        default:
        break;
    }
    */
}


void entity_manager::get_rooms_from_path(std::string& script_path, std::set<std::shared_ptr<entity_wrapper> >& rooms)
{
    auto search = m_room_objs.find(script_path);
    if(search != m_room_objs.end()) 
    {
        for( auto& e : search->second )
        {
            rooms.insert(e);
        }
    }
}

void entity_manager::get_daemons_from_path(std::string& script_path, std::set<std::shared_ptr<entity_wrapper> >& deamons)
{
    auto search = m_daemon_objs.find(script_path);
    if(search != m_daemon_objs.end()) 
    {
        for( auto& e : search->second )
        {
            deamons.insert(e);
        }
    }
}

bool entity_manager::get_parent_env_of_entity(std::string& script_path, sol::environment& env, std::string& env_name)
{
    sol::state & lua = (*m_state);
    std::vector<std::string> strs;
    boost::split(strs, script_path, boost::is_any_of("/"));
    
    // the second to the last will always be the parent,
    // ex/ realms/void, realms is parent
    sol::environment curr_env = lua.globals();
    for( unsigned int x = 0; x < strs.size(); x++ )
    {
        curr_env = curr_env[strs[x]+"_env_"];
        if( x == strs.size() - 2 )
        {
            env = curr_env;
            env_name = strs[x+1]+"_env_";
            return true;
        }
    }
    return false;
}

void entity_manager::reset()
{
    _heartbeat.deregister_all_heartbeat_funcs();
    
    std::vector<std::string> destroy_entities;
    for( auto & e : m_room_objs )
    {
        for( auto & ew : e.second )
        {
            destroy_entities.push_back(ew->script_path);
        }
    }
    
    for( auto & rname : destroy_entities )
        destroy_room( rname );
        
    destroy_entities.clear();
        
    for( auto & e : m_player_objs )
    {
        destroy_entities.push_back(e.second->script_path);
    }
    
    for( auto & pname : destroy_entities )
        destroy_player( pname );
        
    destroy_entities.clear();    
    for( auto & d : m_daemon_objs )
    {
        for( auto & ew : d.second )
        {
            destroy_entities.push_back(ew->script_path);
        }
    }
    
    for( auto & dname : destroy_entities )
        destroy_daemon( dname );
    //m_room_objs.clear();
    
}

roomobj* entity_manager::GetRoomByScriptPath(std::string& script_path, unsigned int & instance_id)
{
    std::set<std::shared_ptr<entity_wrapper> > rooms;
    get_rooms_from_path(script_path, rooms);
    
    for( auto & ew : rooms )
    {
        if( ew->instance_id == instance_id )
        {
            return dynamic_cast<roomobj*>(ew->script_ent);
        }
    }
    return NULL;
}

daemonobj* entity_manager::GetDaemonByScriptPath(std::string& script_path, unsigned int& instance_id)
{
    std::set<std::shared_ptr<entity_wrapper> > dms;
    get_daemons_from_path(script_path, dms);
    
    for( auto & ew : dms )
    {
        if( ew->instance_id == instance_id )
        {
            return dynamic_cast<daemonobj*>(ew->script_ent);
        }
    }
    return NULL;
}

std::vector<commandobj*> entity_manager::GetCommands()
{
    std::vector<commandobj*> ret;
    ret.reserve(m_cmd_objs.size());
    for( auto& kvp : this->m_cmd_objs )
    {
        for( auto& kvp2 : kvp.second)
        {
            commandobj * be = static_cast<commandobj*>(kvp2->script_ent);
            ret.push_back(be);
        }

    }
    return ret;
}

/*
bool entity_manager::get_command(std::string& verb, shared_ptr<entity_wrapper>& cmd)
{
    auto search = command_objs.find(boost::to_lower_copy(verb));
    if(search != command_objs.end()) {
        cmd = search->second;
        return true;
    } else {
        return false;
    }
}
 */


