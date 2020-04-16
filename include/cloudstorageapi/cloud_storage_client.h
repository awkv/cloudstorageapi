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
#include "cloudstorageapi/file_stream.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/logging_client.h"
#include "cloudstorageapi/internal/raw_client.h"
#include "cloudstorageapi/list_folder_reader.h"
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


    template<typename... Options>
    StatusOrVal<FolderMetadata> CreateFolder(std::string const& parentId, std::string const& newName)
    {
        internal::CreateFolderRequest request(parentId, newName);
        return m_RawClient->CreateFolder(request);
    }

    StatusOrVal<FolderMetadata> RenameFolder(std::string const& id,
        std::string const& newName,
        std::string const& parentId,
        std::string const& newParentId)
    {
        internal::RenameRequest request(id, newName, parentId, newParentId);
        return m_RawClient->RenameFolder(request);
    }

    StatusOrVal<FolderMetadata> PatchFolderMetadata(std::string const& folderId,
        FolderMetadata original, FolderMetadata updated)
    {
        internal::PatchFolderMetadataRequest request(folderId, std::move(original), std::move(updated));
        return m_RawClient->PatchFolderMetadata(request);
    }

    // File operations
    /**
     * Return file metadata.
     */
    StatusOrVal<FileMetadata> GetFileMetadata(std::string const& id) const
    {
        internal::GetFileMetadataRequest request(id);
        return m_RawClient->GetFileMetadata(request);
    }

    /**
     * Patches the metadata in a cloud storage.
     *
     * This function creates a patch request to change the writeable attributes in
     * @p original to the values in @p updated. Non-writeable attributes are
     * ignored, and attributes not present in @p updated are removed. Typically
     * this function is used after the application obtained a value with
     * `GetObjectMetadata` and has modified these parameters.
     *
     * @param fileId file's cloud id.
     * @param original the initial value of the object metadata.
     * @param updated the updated value for the object metadata.
     */
    StatusOrVal<FileMetadata> PatchFileMetadata(std::string fileId,
        FileMetadata original, FileMetadata updated)
    {
        internal::PatchFileMetadataRequest request(fileId, std::move(original), std::move(updated));
        return m_RawClient->PatchFileMetadata(request);
    }

    StatusOrVal<FileMetadata> RenameFile(std::string const& id,
                                         std::string const& newName,
                                         std::string const& parentId = "",
                                         std::string const& newParentId = "") // includes move
    {
        internal::RenameRequest request(id, newName, parentId, newParentId);
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

    /**
     * Writes contents into an object.
     *
     * This creates a `std::ostream` object to upload contents. The application
     * can use either the regular `operator<<()`, or `std::ostream::write()` to
     * upload data.
     *
     * The application can explicitly request a new resumable upload using the
     * result of `#NewResumableUploadSession()`. To restore a previously created
     * resumable session use `#RestoreResumableUploadSession()`.
     *
     * @note It is the application's responsibility to query the stream to find
     *     the latest committed byte and to upload data starting from the next
     *     byte expected by the upload session.
     *
     * @note Using the `WithObjectMetadata` option implicitly creates a resumable
     *     upload.
     *
     * Without resumable uploads an interrupted upload has to be restarted from
     * the beginning. Therefore, applications streaming large objects should try
     * to use resumable uploads, and save the session id to restore them if
     * needed.
     *
     * Non-resumable uploads may be more efficient for small and medium sized
     * uploads, as they require fewer roundtrips to the service.
     *
     * For small uploads `InsertFile` is recommended.
     *
     * @param parentId the id of the parent folder that contains the object.
     * @param name the name of the object to be written.
     * @param options a list of optional query parameters and/or request headers.
     *   Valid types for this operation include `ContentEncoding`, `ContentType`,
     *   `UseResumableUploadSession`, and `WithObjectMetadata`.
     */
    template<typename... Options>
    FileWriteStream WriteFile(std::string const& parentId,
                              std::string const& name,
                              Options&&... options)
    {
        internal::ResumableUploadRequest request(parentId, name);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return WriteObjectImpl(request);
    }

    /**
     * Downloads a cloud storage file to a local file.
     *
     * @param parentId the id of the folder that contains the file.
     * @param fileId the id of the file to be downloaded.
     * @param dstFileName the name of the destination file that will have the cloud file
     *   media.
     * @param options a list of optional query parameters and/or request headers.
     *     Valid types for this operation include `ReadFromOffset` and `ReadRange`,
     *     and `ReadLast`.
     *
     */
    template<typename... Options>
    Status DownloadFile(std::string const& fileId,
        std::string const& dstFileName,
        Options&&... options) // TODO: figure out if content type param is needed.
                                        // Maybe for downloading google docs.
    {
        internal::ReadFileRangeRequest request(fileId);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return DownloadFileImpl(request, dstFileName);
    }

    /**
     * Reads the contents of a file.
     *
     * Returns an object derived from `std::istream` which can be used to read the
     * contents of the cloud file. The application should check the `badbit` (e.g.
     * by calling `stream.bad()`) on the returned object to detect if there was
     * an error reading from the file. If `badbit` is set, the application can
     * check the `status()` variable to get details about the failure.
     * Applications can also set the exception mask on the returned stream, in
     * which case an exception is thrown if an error is detected.
     *
     * @param fileId the id of the file to be read.
     * @param options a list of optional query parameters and/or request headers.
     *     Valid types for this operation include `ReadFromOffset`, `ReadRange`
     *     and `ReadLast`.
     */
    template <typename... Options>
    FileReadStream ReadFile(std::string const& fileId,
        Options&&... options)
    {
        struct HasReadRange : public std::disjunction<
            std::is_same<ReadRange, Options>...> {};
        struct HasReadFromOffset : public std::disjunction<
            std::is_same<ReadFromOffset, Options>...> {};
        struct HasReadLast : public std::disjunction<
            std::is_same<ReadLast, Options>...> {};

        struct HasIncompatibleRangeOptions
            : public std::integral_constant<bool, HasReadLast::value &&
            (HasReadFromOffset::value ||
                HasReadRange::value)> {};

        static_assert(!HasIncompatibleRangeOptions::value,
            "Cannot set ReadLast option with either ReadFromOffset or "
            "ReadRange.");

        internal::ReadFileRangeRequest request(fileId);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return ReadObjectImpl(request);
    }
    
    template<typename... Options>
    StatusOrVal<FileMetadata> CopyFile(std::string const& id,
        std::string const& newParentId,
        std::string const& newName,
        Options&&... options)
    {
        internal::CopyFileRequest request(id, newParentId, newName);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return m_RawClient->CopyFileObject(request);
    }

    // TODO: to be implemented.
    //Status DownloadFileThumbnail(std::string const& id, std::string const& dstFileName, uint16_t size = 256, std::string const& imgFormat = "png") const; // Only for box, dropbox and gdrive

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

    FileWriteStream WriteObjectImpl(
        internal::ResumableUploadRequest const& request);

    FileReadStream ReadObjectImpl(
        internal::ReadFileRangeRequest const& request);

    Status DownloadFileImpl(internal::ReadFileRangeRequest const& request,
        std::string const& dstFileName);
};

} // namespace csa
