#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

#include <config.h>
#include <journal.h>
#include <yaml-cpp/yaml.h>

#include <fs_utils.h>
#include <string_utils.h>

#include "common.h"
#include "context.h"

namespace fs = std::filesystem;
namespace fs_utils = filesystem_utils;

namespace app {

    namespace commands {

        auto sys_exec( std::string_view cmd ) {
            using namespace std;

            array<char, 512> buffer;
            string result;
            shared_ptr<FILE> pipe( popen( cmd.data( ), "r" ), pclose );
            if ( !pipe )
                return result;

            while ( !feof( pipe.get( ) ) ) {
                if ( fgets( data( buffer ), size( buffer ), pipe.get( ) ) != nullptr )
                    result += data( buffer );
            }

            return result;
        }

        static auto compile_target( context &ctx, const std::string_view target_path, const std::vector<std::string> &input_files ) {
            using namespace std;
            namespace su = string_utils;

            fs::current_path( ctx.build_path );

            vector<string> compiled_files;

            size_t file_num = 0;
            for ( const auto &f : input_files ) {
                const auto compiled_file = fs::relative( fs::path{f}, fs::path{target_path} ).replace_extension( ".o" );

                const auto work_directory = ctx.build_path / fs::path( compiled_file ).remove_filename( );

                auto work_dir = work_directory.string( );
                work_dir.pop_back( );

                fs::create_directories( work_dir );
                fs::current_path( work_dir );

                // const auto command = ctx.cxx_compiller + " -c -pipe " + f;
                const auto command = su::join( vector<string>{ctx.cxx_compiller, "-c", "-pipe", f}, " " );

                file_num++;

                LOG_INFO( APP_TAG, "[%1/%2] Compile: '%3'", file_num, input_files.size( ), f );

                const auto res = sys_exec( command );
                if ( res.empty( ) ) {
                }

                compiled_files.push_back( compiled_file );
            }

            return compiled_files;
        }

        static auto link_target( context &ctx, const std::string_view target_output, const std::vector<std::string> &input_files ) {
            using namespace std;
            namespace su = string_utils;

            fs::current_path( ctx.build_path );

            const auto all_compiled = su::join( input_files, " " );

            const auto command = ctx.cxx_compiller + " -o " + target_output.data() + " " + all_compiled;

            const auto res = sys_exec( command );
            if ( res.empty( ) ) {
            }

            LOG_INFO( APP_TAG, "Linked: '%1'", target_output );

            return true;
        }

        static auto build_target( context &ctx, const YAML::Node &conf, const std::string_view target_path ) -> bool {
            using namespace std;

            const auto initial_dir = fs::current_path( );

            if ( !ctx.build_path.empty( ) ) {
                const auto target_name = conf["name"].as<string>( );

                vector<string> target_sources;
                if ( conf["sources"].IsScalar( ) ) {
                    const auto sources_value = conf["sources"].as<string>( );
                    if ( sources_value == "all" ) {
                        const auto &extensions = ctx.cxx_extensions;
                        target_sources = fs_utils::get_directory_files_by_ext( string{target_path} + fs::path::preferred_separator + "src" +
                                                                                   fs::path::preferred_separator + target_name,
                                                                               extensions );
                    }
                } else if ( conf["sources"].IsSequence( ) ) {
                    target_sources = conf["sources"].as<vector<string>>( );
                } else {
                    LOG_ERROR( APP_TAG, "Nothing to build for target %1", target_name );
                }

                namespace su = string_utils;

                const auto compiled_files = compile_target( ctx, target_path, target_sources );

                const auto all_compiled = su::join( compiled_files, " " );

//                // Link files
//                fs::current_path( ctx.build_path );
//                {
//                    const auto command = ctx.cxx_compiller + " -o " + target_name + " " + all_compiled;

//                    const auto res = sys_exec( command );
//                    if ( res.empty( ) ) {
//                    }

//                    LOG_INFO( APP_TAG, "Linked: '%1'", target_name );
//                }

                link_target( ctx, target_name, compiled_files );

                fs::current_path( initial_dir );

                return true;
            }

            LOG_ERROR( APP_TAG, "%1", "No build directory" );

            return false;
        }

        auto build_all( context &ctx, std::string_view arguments ) -> bool {

            const auto target_path = fs::current_path( ).string( );
            const auto conf_path = target_path + fs::path::preferred_separator + DEFAULT_BUMP_FILE;

            if ( fs::exists( conf_path ) ) {
                auto conf = YAML::LoadFile( conf_path );

                fs::create_directory( ctx.build_path );

                const auto project_name = conf["project"].as<std::string>( );

                LOG_INFO( APP_TAG, "Build '%1'", project_name );

                auto root_targets = conf["build"];
                for ( const auto &target : root_targets ) {
                    build_target( ctx, target, target_path );
                }

                LOG_INFO( APP_TAG, "Build '%1' done", project_name );

                return true;
            }

            LOG_ERROR( APP_TAG, "%1", "No config for build" );

            return false;
        }

    } // namespace commands

} // namespace app
