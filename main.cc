#include "spectre.h"

using namespace spctr;
using json = nlohmann::json;

int main()
{
        std::ifstream f("./token.json");
        if (!f.is_open())
                std::cerr << "Unable to open file";
        json data = json::parse(f);
        f.close();
        const std::string BOT_TOKEN = data["token"].get<std::string>();
        soul obj{BOT_TOKEN, INTENTS::MESSAGE_CONTENT | INTENTS::GUILD_MESSAGES | INTENTS::GUILDS};
        obj.form();
}
