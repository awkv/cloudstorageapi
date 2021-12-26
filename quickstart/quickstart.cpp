// Copyright 2021 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/cloud_storage_client.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Missing file name.\n";
        std::cerr << "Usage: quickstart <file-name>\n";
        return 1;
    }
    std::string const file_name = argv[1];

    // Create a client to communicate with Google Drive. This client
    // uses the default configuration for authentication.
    csa::StatusOrVal<csa::CloudStorageClient> client =
        csa::CloudStorageClient(csa::Options{}.Set<csa::ProviderOption>(csa::EProvider::GoogleDrive));
    if (!client)
    {
        std::cerr << "Failed to create cloud storage client, status=" << client.GetStatus() << std::endl;
        return 1;
    }

    auto root_meta = client->GetFolderMetadata("root");
    if (!root_meta)
    {
        std::cerr << "Failed to get root folder metadata: " << root_meta.GetStatus().Message();
    }

    auto writer = client->WriteFile(root_meta->GetCloudId(), file_name);
    writer << "Hello World!";
    writer.Close();
    if (writer.GetMetadata())
    {
        std::cout << "Successfully created object: " << *writer.GetMetadata() << "\n";
    }
    else
    {
        std::cerr << "Error creating object: " << writer.GetMetadata().GetStatus() << "\n";
        return 1;
    }

    auto file_id = writer.GetMetadata().Value().GetCloudId();
    auto reader = client->ReadFile(file_id);
    if (!reader)
    {
        std::cerr << "Error reading object: " << reader.GetStatus() << "\n";
        return 1;
    }

    std::string contents{std::istreambuf_iterator<char>{reader}, {}};
    std::cout << contents << "\n";

    return 0;
}
