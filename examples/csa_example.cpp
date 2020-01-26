// Copyright 2019 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/cloud_storage_client.h"
#include <functional>

namespace {
class NeedUsage : public std::exception
{
public:
    NeedUsage() : std::exception() {}
    NeedUsage(const char* const msg) : std::exception(msg) {}
    NeedUsage(std::string const& msg) : std::exception(msg.c_str()) {}
};

std::string commandUsage;

void PrintUsage(int argc, char* argv[], std::string const& msg)
{
    std::string const cmd = argv[0];
    auto lastSlash = cmd.find_last_of('/');
    if (lastSlash == std::string::npos)
        lastSlash = cmd.find_last_of('\\');
    auto program = cmd.substr(lastSlash + 1);
    std::cerr << msg << "\nUsage: " << program << " <provider> <command> [arguments]\n\n"
        << "Commands:\n"
        << commandUsage << "\n";
}

char const* ConsumeArg(int& argc, char* argv[])
{
    if (argc < 2)
    {
        return nullptr;
    }

    char const* result = argv[1];
    std::copy(argv + 2, argv + argc, argv + 1);
    argc--;
    return result;
}

void ListFolder(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("list-folder <folder-path>");

    auto folderName = ConsumeArg(argc, argv);
    for (auto&& metadata : client->ListFolder(folderName))
    {
        if (!metadata)
            throw std::runtime_error(metadata.GetStatus().Message());

        std::visit([](auto&& item) {
            std::cout << item.GetName() << std::endl <<
            "\t" << item << std::endl << std::endl; }, *metadata);
    }
}

void ListFolderWithPageSize(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 3)
        throw NeedUsage("list-folder-with-page-size <folder-path> <page-size>");

    auto folderName = ConsumeArg(argc, argv);
    int pageSize = std::stoi(ConsumeArg(argc, argv));
    for (auto&& metadata : client->ListFolder(folderName, csa::PageSize(pageSize)))
    {
        if (!metadata)
            throw std::runtime_error(metadata.GetStatus().Message());

        std::visit([](auto&& item) {
            std::cout << item.GetName() << std::endl <<
                "\t" << item << std::endl << std::endl; }, *metadata);
    }

}

void GetFolderMetadata(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("get-folder-metadata <folder-path>");
    
    auto folderName = ConsumeArg(argc, argv);
    csa::StatusOrVal<csa::FolderMetadata> folderMetadata = client->GetFolderMetadata(folderName);
    if (!folderMetadata)
    {
        throw std::runtime_error(folderMetadata.GetStatus().Message());
    }

    std::cout << "\'" << folderMetadata->GetName() << "\' " <<
        *folderMetadata << "\n";
}

void GetFileMetadata(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("get-file-metadata <file-path>");

    auto fileName = ConsumeArg(argc, argv);
    csa::StatusOrVal<csa::FileMetadata> fileMetadata = client->GetFileMetadata(fileName);
    if (!fileMetadata)
    {
        throw std::runtime_error(fileMetadata.GetStatus().Message());
    }

    std::cout << "\'" << fileMetadata->GetName() << "\' " <<
        *fileMetadata << "\n";
}

void RenameFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || (argc != 3 && argc != 5))
        throw NeedUsage("rename-file <file-id> <new name> [<parent-id> <new-parent-id>]");

    auto fileId = ConsumeArg(argc, argv);
    auto newName = ConsumeArg(argc, argv);
    std::string parentId;
    std::string newParentid;
    if (argc == 3)// total 5, 2 are consumed above.
    {
        parentId = ConsumeArg(argc, argv);
        newParentid = ConsumeArg(argc, argv);
    }
    csa::StatusOrVal<csa::FileMetadata> fileMetadata = client->RenameFile(fileId, newName, parentId, newParentid);
    if (!fileMetadata)
    {
        throw std::runtime_error(fileMetadata.GetStatus().Message());
    }

    std::cout << "Rename file succeeded: id=\'" << fileId << "\' "
        << *fileMetadata << std::endl;
}
} // namespace

int main(int argc, char* argv[]) try 
{
    using CmdFn = std::function<void(csa::CloudStorageClient*, int&, char*[])>;
    std::map<std::string, CmdFn> cmdMap = {
        {"list-folder", &ListFolder},
        {"list-folder-with-page-size", &ListFolderWithPageSize},
        {"get-folder-metadata", &GetFolderMetadata},
        {"get-file-metadata", &GetFileMetadata},
        {"rename-file", &RenameFile}
    };
    for (auto&& cmd : cmdMap)
    {
        try {
            int fakeArgc = 0;
            cmd.second(nullptr, fakeArgc, argv);
        }
        catch (NeedUsage const& ex)
        {
            commandUsage += "\t";
            commandUsage += ex.what();
            commandUsage += "\n";
        }
    }

    if (argc < 3)
    {
        throw NeedUsage("Missing provider name and/or command.");
    }

    std::string provider = ConsumeArg(argc, argv);
    auto provider_it = std::find_if(csa::ProviderNames.begin(), csa::ProviderNames.end(), [&provider](auto& item) {
        return item.second == provider; });
    if (provider_it == std::end(csa::ProviderNames))
    {
        std::cerr << "Unknown provider: " << provider << std::endl;
        return 1;
    }
    // Create a cloud storage client.
    csa::StatusOrVal<csa::CloudStorageClient> client =
        csa::CloudStorageClient::CreateDefaultClient(csa::EProvider::GoogleDrive);
    if (!client)
    {
        std::cerr << "Failed to create cloud storage client, status=" << client.GetStatus()
            << std::endl;
        return 1;
    }

    std::string cmd = ConsumeArg(argc, argv);
    auto cmdIt = cmdMap.find(cmd);
    if (cmdIt == std::end(cmdMap))
    {
        throw NeedUsage("Unknown command: " + cmd);
    }

    cmdIt->second(&*client, argc, argv);

    return 0;
}
catch (NeedUsage const& ex)
{
    PrintUsage(argc, argv, ex.what());
    return 0;
}
catch (std::exception const& ex)
{
    std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
    return 1;
}
