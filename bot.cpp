#include <dpp/dpp.h>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <regex>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <json/json.h>

// Forward declarations
class MusicBot;
struct Track;

// Track structure to hold song information
struct Track {
    std::string title;
    std::string url;
    std::string stream_url;
    std::string thumbnail;
    std::string requester_id;
    std::string requester_mention;
    int duration = 0;
    
    Track() = default;
    Track(const std::string& t, const std::string& u, const std::string& s, 
          const std::string& thumb, const std::string& req_id, 
          const std::string& req_mention, int dur = 0)
        : title(t), url(u), stream_url(s), thumbnail(thumb), 
          requester_id(req_id), requester_mention(req_mention), duration(dur) {}
};

// Guild music state
struct GuildMusicState {
    std::queue<Track> queue;
    std::shared_ptr<Track> current_track = nullptr;
    int loop_mode = 0; // 0 = off, 1 = track, 2 = queue
    std::chrono::steady_clock::time_point start_time;
    bool is_playing = false;
    bool is_paused = false;
    
    void clear() {
        while (!queue.empty()) queue.pop();
        current_track = nullptr;
        loop_mode = 0;
        is_playing = false;
        is_paused = false;
    }
};

class MusicBot {
private:
    std::map<dpp::snowflake, GuildMusicState> guild_states;
    
public:
    GuildMusicState& get_guild_state(dpp::snowflake guild_id) {
        if (guild_states.find(guild_id) == guild_states.end()) {
            guild_states[guild_id] = GuildMusicState();
        }
        return guild_states[guild_id];
    }
    
    void clear_guild_state(dpp::snowflake guild_id) {
        auto it = guild_states.find(guild_id);
        if (it != guild_states.end()) {
            it->second.clear();
            guild_states.erase(it);
        }
    }
};

// Global bot instance
MusicBot music_bot;

// YouTube-DL wrapper function
std::vector<Track> extract_audio_info(const std::string& query, const std::string& requester_id, const std::string& requester_mention) {
    std::vector<Track> tracks;
    
    // Create a temporary file for yt-dlp output
    std::string temp_file = "/tmp/ytdl_output_" + std::to_string(std::time(nullptr)) + ".json";
    
    // Prepare the search query
    std::string search_query = query;
    if (query.find("http://") != 0 && query.find("https://") != 0) {
        search_query = "ytsearch:" + query;
    }
    
    // Execute yt-dlp command
    std::string command = "yt-dlp --dump-json --no-playlist --format bestaudio/best \"" + 
                         search_query + "\" > " + temp_file + " 2>/dev/null";
    
    int result = std::system(command.c_str());
    
    if (result == 0) {
        std::ifstream file(temp_file);
        std::string line;
        
        while (std::getline(file, line)) {
            if (!line.empty()) {
                Json::Value root;
                Json::Reader reader;
                
                if (reader.parse(line, root)) {
                    Track track;
                    track.title = root.get("title", "Unknown Title").asString();
                    track.url = root.get("webpage_url", "").asString();
                    track.stream_url = root.get("url", "").asString();
                    track.thumbnail = root.get("thumbnail", "").asString();
                    track.duration = root.get("duration", 0).asInt();
                    track.requester_id = requester_id;
                    track.requester_mention = requester_mention;
                    
                    if (!track.stream_url.empty()) {
                        tracks.push_back(track);
                    }
                }
            }
        }
        file.close();
    }
    
    // Clean up temporary file
    std::filesystem::remove(temp_file);
    
    return tracks;
}

// Format duration function
std::string format_duration(int seconds) {
    if (seconds <= 0) return "N/A";
    
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (hours > 0) {
        return std::to_string(hours) + ":" + 
               (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
               (secs < 10 ? "0" : "") + std::to_string(secs);
    } else {
        return std::to_string(minutes) + ":" + 
               (secs < 10 ? "0" : "") + std::to_string(secs);
    }
}

// Play next track function
void play_next(dpp::cluster& bot, dpp::snowflake guild_id, dpp::snowflake channel_id);

// Bot event handlers and commands
int main() {
    // Load environment variables
    const char* token = std::getenv("DISCORD_TOKEN");
    if (!token) {
        std::cerr << "DISCORD_TOKEN environment variable not set!" << std::endl;
        return 1;
    }
    
    // Create bot cluster
    dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content);
    
    // Logging
    bot.on_log(dpp::utility::cout_logger());
    
    // Bot ready event
    bot.on_ready([&bot](const dpp::ready_t& event) {
        std::cout << "Logged in as " << bot.me.username << "!" << std::endl;
        
        if (dpp::run_once<struct register_bot_commands>()) {
            // Register slash commands
            bot.global_command_create(dpp::slashcommand("play", "Play music from YouTube", bot.me.id)
                .add_option(dpp::command_option(dpp::co_string, "query", "YouTube URL or search query", true)));
                
            bot.global_command_create(dpp::slashcommand("skip", "Skip the current track", bot.me.id));
            bot.global_command_create(dpp::slashcommand("stop", "Stop playback and clear queue", bot.me.id));
            bot.global_command_create(dpp::slashcommand("pause", "Pause the current track", bot.me.id));
            bot.global_command_create(dpp::slashcommand("resume", "Resume playback", bot.me.id));
            bot.global_command_create(dpp::slashcommand("queue", "Show the current queue", bot.me.id));
            bot.global_command_create(dpp::slashcommand("clear", "Clear the queue", bot.me.id));
            bot.global_command_create(dpp::slashcommand("nowplaying", "Show current track", bot.me.id));
            
            bot.global_command_create(dpp::slashcommand("loop", "Set loop mode", bot.me.id)
                .add_option(dpp::command_option(dpp::co_string, "mode", "Loop mode: off, track, queue", true)
                    .add_choice(dpp::command_option_choice("off", std::string("off")))
                    .add_choice(dpp::command_option_choice("track", std::string("track")))
                    .add_choice(dpp::command_option_choice("queue", std::string("queue")))));
                    
            bot.global_command_create(dpp::slashcommand("remove", "Remove track from queue", bot.me.id)
                .add_option(dpp::command_option(dpp::co_integer, "position", "Track position in queue", true)));
        }
    });
    
    // Slash command handler
    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "play") {
            // Get user's voice channel
            dpp::guild* g = dpp::find_guild(event.command.guild_id);
            if (!g) {
                event.reply("❌ Guild not found!");
                return;
            }
            
            auto vs = g->voice_members.find(event.command.get_issuing_user().id);
            if (vs == g->voice_members.end()) {
                event.reply("❌ You need to be in a voice channel first!");
                return;
            }
            
            dpp::snowflake user_voice_channel = g->voice_members[event.command.get_issuing_user().id].channel_id;
            
            // Get query parameter
            std::string query = std::get<std::string>(event.get_parameter("query"));
            
            event.reply("🔄 Searching and processing: `" + query + "`...");
            
            // Connect to voice channel if not already connected
            if (!g->connecting_voice_channel && !g->voice_channel) {
                bot.connect_voice(event.command.guild_id, user_voice_channel);
            }
            
            // Extract audio info in a separate thread
            std::thread([&bot, query, event, user_voice_channel]() {
                auto tracks = extract_audio_info(query, 
                    std::to_string(event.command.get_issuing_user().id),
                    event.command.get_issuing_user().get_mention());
                
                if (tracks.empty()) {
                    bot.interaction_followup_edit_original(event.command.token, 
                        dpp::message("❌ No playable tracks found!"));
                    return;
                }
                
                auto& state = music_bot.get_guild_state(event.command.guild_id);
                
                // Add tracks to queue
                for (const auto& track : tracks) {
                    state.queue.push(track);
                }
                
                std::string response = "✅ Added " + std::to_string(tracks.size()) + 
                                     " track" + (tracks.size() > 1 ? "s" : "") + " to the queue!";
                
                // Start playing if nothing is currently playing
                if (!state.is_playing) {
                    play_next(bot, event.command.guild_id, event.command.channel_id);
                }
                
                bot.interaction_followup_edit_original(event.command.token, dpp::message(response));
            }).detach();
        }
        else if (event.command.get_command_name() == "skip") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            if (!state.is_playing) {
                event.reply("❌ Nothing is playing right now!");
                return;
            }
            
            // Stop current track (this will trigger play_next)
            dpp::discord_voice_client* v = event.from->get_voice(event.command.guild_id);
            if (v && v->voiceclient && v->voiceclient->is_ready()) {
                v->voiceclient->stop_audio();
            }
            
            event.reply("⏭️ Skipped!");
        }
        else if (event.command.get_command_name() == "stop") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            // Clear state
            music_bot.clear_guild_state(event.command.guild_id);
            
            // Disconnect from voice
            dpp::discord_voice_client* v = event.from->get_voice(event.command.guild_id);
            if (v && v->voiceclient && v->voiceclient->is_ready()) {
                v->voiceclient->stop_audio();
            }
            
            event.from->disconnect_voice(event.command.guild_id);
            event.reply("⏹️ Playback stopped, queue cleared, and disconnected.");
        }
        else if (event.command.get_command_name() == "pause") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            if (!state.is_playing || state.is_paused) {
                event.reply("❌ Nothing is currently playing or already paused!");
                return;
            }
            
            dpp::discord_voice_client* v = event.from->get_voice(event.command.guild_id);
            if (v && v->voiceclient && v->voiceclient->is_ready()) {
                v->voiceclient->pause_audio(true);
                state.is_paused = true;
            }
            
            event.reply("⏸️ Paused.");
        }
        else if (event.command.get_command_name() == "resume") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            if (!state.is_paused) {
                event.reply("❌ The track is not paused!");
                return;
            }
            
            dpp::discord_voice_client* v = event.from->get_voice(event.command.guild_id);
            if (v && v->voiceclient && v->voiceclient->is_ready()) {
                v->voiceclient->pause_audio(false);
                state.is_paused = false;
            }
            
            event.reply("▶️ Resumed.");
        }
        else if (event.command.get_command_name() == "queue") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            dpp::embed embed = dpp::embed()
                .set_title("Music Queue")
                .set_color(0x0099ff)
                .set_timestamp(time(0));
            
            // Current track
            if (state.current_track) {
                std::string current_info = "**[" + state.current_track->title + "](" + 
                                         state.current_track->url + ")**\n";
                current_info += "Duration: " + format_duration(state.current_track->duration) + "\n";
                current_info += "Requested by: " + state.current_track->requester_mention;
                embed.add_field("🎵 Now Playing", current_info, false);
            } else {
                embed.add_field("🎵 Now Playing", "Nothing", false);
            }
            
            // Queue
            if (!state.queue.empty()) {
                std::string queue_text = "";
                std::queue<Track> temp_queue = state.queue;
                int position = 1;
                
                while (!temp_queue.empty() && position <= 10) {
                    Track track = temp_queue.front();
                    temp_queue.pop();
                    
                    std::string title = track.title;
                    if (title.length() > 45) {
                        title = title.substr(0, 45) + "...";
                    }
                    
                    queue_text += "`" + std::to_string(position) + ".` **[" + title + "](" + 
                                track.url + ")** | " + format_duration(track.duration) + 
                                " | Req: " + track.requester_mention + "\n";
                    position++;
                }
                
                if (state.queue.size() > 10) {
                    queue_text += "\n*...and " + std::to_string(state.queue.size() - 10) + 
                                 " more track(s).*\nTotal length: " + std::to_string(state.queue.size()) + " songs";
                }
                
                embed.add_field("📑 Up Next (" + std::to_string(state.queue.size()) + " tracks)", 
                              queue_text, false);
            } else {
                embed.add_field("📑 Up Next", "Queue is empty", false);
            }
            
            // Loop mode
            std::vector<std::string> loop_modes = {"Disabled", "Single Track", "Queue"};
            embed.set_footer("Loop Mode: " + loop_modes[state.loop_mode], "");
            
            event.reply(dpp::message().add_embed(embed));
        }
        else if (event.command.get_command_name() == "clear") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            if (state.queue.empty()) {
                event.reply("❌ The queue is already empty!");
                return;
            }
            
            size_t queue_size = state.queue.size();
            while (!state.queue.empty()) {
                state.queue.pop();
            }
            
            event.reply("🗑️ Cleared " + std::to_string(queue_size) + " tracks from the queue!");
        }
        else if (event.command.get_command_name() == "loop") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            std::string mode = std::get<std::string>(event.get_parameter("mode"));
            
            if (mode == "off") {
                state.loop_mode = 0;
                event.reply("🔄 Loop mode set to: **Disabled**");
            } else if (mode == "track") {
                state.loop_mode = 1;
                event.reply("🔂 Loop mode set to: **Single Track**");
            } else if (mode == "queue") {
                state.loop_mode = 2;
                event.reply("🔁 Loop mode set to: **Queue**");
            }
        }
        else if (event.command.get_command_name() == "nowplaying") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            
            if (!state.current_track || !state.is_playing) {
                event.reply("❌ Nothing is playing right now!");
                return;
            }
            
            dpp::embed embed = dpp::embed()
                .set_title("Now Playing")
                .set_color(0x0099ff)
                .set_description("**[" + state.current_track->title + "](" + state.current_track->url + ")**");
            
            // Calculate progress
            auto now = std::chrono::steady_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::seconds>(now - state.start_time);
            int position = duration_ms.count();
            
            if (state.current_track->duration > 0) {
                std::string progress_bar = "`[";
                double percent = (double)position / state.current_track->duration;
                int filled_blocks = (int)(percent * 10);
                
                for (int i = 0; i < 10; i++) {
                    progress_bar += (i < filled_blocks) ? "▬" : "─";
                }
                progress_bar += "]`";
                
                std::string time_text = progress_bar + "\n" + 
                                      format_duration(position) + " / " + 
                                      format_duration(state.current_track->duration);
                embed.add_field("Time", time_text, false);
            } else {
                embed.add_field("Time", "Live Stream or Duration Unknown", false);
            }
            
            embed.add_field("Requested by", state.current_track->requester_mention, true);
            
            std::vector<std::string> loop_modes = {"Disabled", "Single Track", "Queue"};
            embed.add_field("Loop Mode", loop_modes[state.loop_mode], true);
            
            if (!state.current_track->thumbnail.empty()) {
                embed.set_thumbnail(state.current_track->thumbnail);
            }
            
            event.reply(dpp::message().add_embed(embed));
        }
        else if (event.command.get_command_name() == "remove") {
            auto& state = music_bot.get_guild_state(event.command.guild_id);
            int position = std::get<int64_t>(event.get_parameter("position"));
            
            if (position < 1 || position > (int)state.queue.size()) {
                event.reply("❌ Invalid track number. Must be between 1 and " + 
                          std::to_string(state.queue.size()) + ".");
                return;
            }
            
            // Convert queue to vector for easier manipulation
            std::vector<Track> tracks;
            while (!state.queue.empty()) {
                tracks.push_back(state.queue.front());
                state.queue.pop();
            }
            
            Track removed_track = tracks[position - 1];
            tracks.erase(tracks.begin() + position - 1);
            
            // Rebuild queue
            for (const auto& track : tracks) {
                state.queue.push(track);
            }
            
            event.reply("✂️ Removed track #" + std::to_string(position) + ": **" + 
                       removed_track.title + "**");
        }
    });
    
    // Voice state update handler
    bot.on_voice_state_update([&bot](const dpp::voice_state_update_t& event) {
        // Handle voice state changes if needed
    });
    
    // Start the bot
    bot.start(dpp::st_wait);
    
    return 0;
}

// Play next track implementation
void play_next(dpp::cluster& bot, dpp::snowflake guild_id, dpp::snowflake channel_id) {
    auto& state = music_bot.get_guild_state(guild_id);
    
    dpp::discord_voice_client* v = bot.get_voice(guild_id);
    if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
        return;
    }
    
    std::shared_ptr<Track> next_track = nullptr;
    
    // Handle loop modes
    if (state.loop_mode == 1 && state.current_track) {
        // Single track loop
        next_track = state.current_track;
    } else {
        if (state.loop_mode == 2 && state.current_track) {
            // Queue loop - add current track back to queue
            state.queue.push(*state.current_track);
        }
        
        if (!state.queue.empty()) {
            next_track = std::make_shared<Track>(state.queue.front());
            state.queue.pop();
        } else {
            // Queue finished
            state.is_playing = false;
            state.current_track = nullptr;
            bot.message_create(dpp::message(channel_id, "📪 Queue finished."));
            return;
        }
    }
    
    if (!next_track) {
        return;
    }
    
    state.current_track = next_track;
    state.start_time = std::chrono::steady_clock::now();
    state.is_playing = true;
    state.is_paused = false;
    
    // Send now playing embed
    dpp::embed embed = dpp::embed()
        .set_title("Now Playing")
        .set_description("🎵 **[" + next_track->title + "](" + next_track->url + ")**")
        .set_color(0x0099ff);
    
    embed.add_field("Requested by", next_track->requester_mention, true);
    if (next_track->duration > 0) {
        embed.add_field("Duration", format_duration(next_track->duration), true);
    }
    if (!next_track->thumbnail.empty()) {
        embed.set_thumbnail(next_track->thumbnail);
    }
    
    std::vector<std::string> loop_modes = {"Off", "Track", "Queue"};
    embed.set_footer("Loop: " + loop_modes[state.loop_mode], "");
    
    bot.message_create(dpp::message(channel_id, "").add_embed(embed));
    
    // Play the audio
    v->voiceclient->play_audio_file(next_track->stream_url, [&bot, guild_id, channel_id](const std::string& error) {
        if (!error.empty()) {
            bot.message_create(dpp::message(channel_id, "❌ Playback error: " + error));
        }
        
        // Play next track when current finishes
        auto& current_state = music_bot.get_guild_state(guild_id);
        if (current_state.loop_mode != 1) {
            play_next(bot, guild_id, channel_id);
        } else {
            play_next(bot, guild_id, channel_id); // For single track loop
        }
    });
}