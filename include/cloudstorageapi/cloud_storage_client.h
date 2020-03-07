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

#pragma once

#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/list_folder_reader.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/logging_client.h"
#include "cloudstorageapi/internal/raw_client.h"
#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/upload_options.h"

namespace csa {

namespace internal { class RawClient; }
namespace auth { class Credentials; }

/**
 * Cloud storage client.
 *
 * This is a main class to interact with cloud storage. It is generalization of different cloud storages APIs.
 * It provides member functions to work with cloud storages independently. Some functionality might not be supported
 * by all cloud storages. In this case those functions return empty or default result. See documentation for each
 * function for details.
 */
class CloudStorageClient
{
public:

    explicit CloudStorageClient(ClientOptions options)
        : CloudStorageClient(CreateDefaultRawClient(std::move(options)))
    {}

    explicit CloudStorageClient(EProvider provider,
        std::shared_ptr<auth::Credentials> credentials)
        : CloudStorageClient(ClientOptions(provider, std::move(credentials)))
    {}

    explicit CloudStorageClient(std::shared_ptr<internal::RawClient> client)
    {
        if (client->GetClientOptions().GetEnableRawClientTracing())
            m_RawClient = std::make_shared<internal::LoggingClient>(std::move(client));
        else
            m_RawClient = std::move(client);
    }

    static StatusOrVal<CloudStorageClient> CreateDefaultClient(EProvider provider);

    /**
     * \brief Get the provider name.
     *
     * @return The provider name (ie. "googledrive", "dropbox", ...)
     */
    std::string GetProviderName() const { return m_RawClient->GetProviderName(); }

    // TODO: to be implemented
    ///**
    // * \brief Get user identifier.
    // *
    // * @return user identifier (login in case of login/password,
    // *         or email in case of OAuth.
    // */
    //std::string GetUserId() const;

    // Common operations (folders and files)
    //StatusOrVal<bool> Exists(const std::string& path) const;

    /**
     * Delete object (file or folder) by given id.
     * it deletes object permanently and recursively at least for gdrive.
     *
     * TODO: if needed provide 'bool permanent' and 'bool recursive' parameters.
     *       Permanent is only possible for box, gdrive, sharepoint, onedrivebiz and sharefile
     */
    Status Delete(std::string const& id)
    {
        internal::DeleteRequest request(id);
        return m_RawClient->Delete(request).GetStatus();
    }

    // Folder operations
    /**
     * Returns list of objects metadata located in given folder.
     * 
     * @param id an id of a object as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     *
     * @param options a list of optional query parameters and/or request headers.
     *     Valid types for this operation include `PageSize`.
     */
    template<typename... Options>
    ListFolderReader ListFolder(std::string const& id, Options&&... options) const
    {
        internal::ListFolderRequest request(id);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        auto client = m_RawClient;
        return ListFolderReader(request,
            [client](internal::ListFolderRequest const& r)
            {
                return client->ListFolder(r);
            });
    }

    /**
     * Returns object's metadata.
     */
    StatusOrVal<FolderMetadata> GetFolderMetadata(std::string const& folderId) const
    {
        internal::GetFolderMetadataRequest request(folderId);
        return m_RawClient->GetFolderMetadata(request);
    }


    // TODO: to me implemented
    //StatusOrVal<FolderMetadata> CreateFolder(std::string const& parentId, std::string const& newName, bool conflictIfExists);
    //StatusOrVal<FolderMetadata> RenameFolder(std::string const& id, std::string const& newParentId, std::string const& newName);
    //StatusOrVal<FolderMetadata> CopyFolder(std::string const& id, std::string const& newParentId, std::string const& newName); // Some limitation, not all clouds support it
    //StatusOrVal<FolderMetadata> UpdateFolder(std::string const& id, FolderMetadata metadata);

    // File operations
    /**
     * Return file metadata.
     */
    StatusOrVal<FileMetadata> GetFileMetadata(std::string const& id) const
    {
        internal::GetFileMetadataRequest request(id);
        return m_RawClient->GetFileMetadata(request);
    }

    StatusOrVal<FileMetadata> RenameFile(std::string const& id,
                                         std::string const& newName,
                                         std::string const& parentId = "",
                                         std::string const& newParentId = "") // includes move
    {
        internal::RenameFileRequest request(id, newName, parentId, newParentId);
        return m_RawClient->RenameFile(request);
    }

    /**
     * Creates an object given its name and contents.
     */
    template <typename... Options>
    StatusOrVal<FileMetadata> InsertFile(std::string const& folderId,
                                         std::string const& name,
                                         std::string content,
                                         Options&&... options)
    {
        internal::InsertFileRequest request(folderId, name, std::move(content));
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return m_RawClient->InsertFile(request);
    }

    /**
     *  Uploads given local file to the storage.
     *
     * @note
     * Only regular files are supported. If you need to upload the results of
     * reading a device, Named Pipe, FIFO, or other type of file system object
     * that is **not** a regular file then `WriteObject()` is probably a better
     * alternative.
     */
    template<typename... Options>
    StatusOrVal<FileMetadata> UploadFile(std::string const& srcFileName,
                                         std::string const& parentId,
                                         std::string const& name,
                                         //bool overwrite,
                                         Options&&... options)// Use multipart upload if size > 100MB
    {
        // Determine, at compile time, which version of UploadFileImpl we should
        // call. This needs to be done at compile time because InsertFile
        // does not support (nor should it support) the UseResumableUploadSession
        // option.
        using HasUseResumableUpload = std::disjunction<
            std::is_same<UseResumableUploadSession, Options>...>;
        return UploadFileImpl(srcFileName, parentId, name, HasUseResumableUpload{}, std::forward<Options>(options)...);
    }

    // TODO: to be implemented.
    //StatusOrVal<FileWriteStream> WriteFile(std::string const& id); // TODO: add parameters like offset, range, etc.
    //StatusOrVal<FileMetadata> UpdateFile(std::string const& id, FileMetadata metadata);
    //Status DownloadFile(std::string const& id, std::string const& dstFileName, std::string const& contentTypeOpt) const;
    //Status DownloadFileThumbnail(std::string const& id, std::string const& dstFileName, uint16_t size = 256, std::string const& imgFormat = "png") const; // Only for box, dropbox and gdrive
    //StatusOrVal<FileReadStream> ReadFile(std::string const& id) const;
    //StatusOrVal<FileMetadata> CopyFile(std::string const& id, std::string const& newParentId, std::string const& newNameOpt);

    // TODO: to be implemented.
    //// Multipart uploading operations
    //MultipartID InitializeMultipartUploadSession(new_name, parent_id, size, overwrite: bool, parallel : bool);
    //MultipartInfo<id, part_size, parallel> GetMultipartSessionInfo(MultipartID);
    //void MultipartUploadPart(MultipartID, part_number);
    //File CompleteMultipartSession(MultipartID);
    //void DeleteMultipartSession(MultipartID);

    // TODO: decide if links operations are needed.
    // Links operations
    //Array<Link> List(active: bool, page_size, page);
    //Link Create(file_cloud_id, password_opt, expiration_opt, direct_opt);
    //Link Get(id, active);
    //Link Update(id, active, expiration, password);
    //void Delete(id);

    // TODO: to be implemented.
    // Quota operations
    // @return <total, used>
    //std::pair<std::size_t, std::size_t> GetQuota();

private:

    std::shared_ptr<internal::RawClient> m_RawClient;

    CloudStorageClient() = default;

    static std::shared_ptr<internal::RawClient> CreateDefaultRawClient(
        ClientOptions options);

    // The version of UploadFile() where UseResumableUploadSession is one of the
    // options. Note how this does not use InsertFile at all.
    template <typename... Options>
    StatusOrVal<FileMetadata> UploadFileImpl(std::string const& srcFileName,
        std::string const& parentId,
        std::string const& name,
        std::true_type,
        Options&&... options)
    {
        internal::ResumableUploadRequest request(parentId, name);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return UploadFileResumable(srcFileName, request);
    }

    // The version of UploadFile() where UseResumableUploadSession is *not* one of
    // the options. In this case we can use InsertFileRequest because it
    // is safe.
    template <typename... Options>
    StatusOrVal<FileMetadata> UploadFileImpl(std::string const& srcFileName,
        std::string const& parentId,
        std::string const& name,
        std::false_type,
        Options&&... options)
    {
        if (UseSimpleUpload(srcFileName))
        {
            internal::InsertFileRequest request(parentId, name,
                std::string{});
            request.SetMultipleOptions(std::forward<Options>(options)...);
            return UploadFileSimple(srcFileName, request);
        }
        internal::ResumableUploadRequest request(parentId, name);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return UploadFileResumable(srcFileName, request);
    }

    bool UseSimpleUpload(std::string const& fileName) const;

    StatusOrVal<FileMetadata> UploadFileSimple(
        std::string const& fileName, internal::InsertFileRequest request);

    StatusOrVal<FileMetadata> UploadFileResumable(
        std::string const& fileName,
        internal::ResumableUploadRequest const& request);

    StatusOrVal<FileMetadata> UploadStreamResumable(
        std::istream& source, internal::ResumableUploadRequest const& request);
};

} // namespace csa
