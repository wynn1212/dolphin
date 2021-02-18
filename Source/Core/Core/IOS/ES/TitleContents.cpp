// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <utility>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE
{
s32 ESDevice::OpenContent(const ES::TMDReader& tmd, u16 content_index, u32 uid)
{
  const u64 title_id = tmd.GetTitleId();

  ES::Content content;
  if (!tmd.GetContent(content_index, &content))
    return ES_EINVAL;

  for (size_t i = 0; i < m_content_table.size(); ++i)
  {
    OpenedContent& entry = m_content_table[i];
    if (entry.m_opened)
      continue;

    auto file = m_ios.GetFS()->OpenFile(PID_KERNEL, PID_KERNEL, GetContentPath(title_id, content),
                                        FS::Mode::Read);
    if (!file)
      return FS::ConvertResult(file.Error());

    entry.m_opened = true;
    entry.m_fd = file->Release();
    entry.m_content = content;
    entry.m_title_id = title_id;
    entry.m_uid = uid;
    INFO_LOG_FMT(IOS_ES, "OpenContent: title ID {:016x}, UID {:#x}, CFD {}", title_id, uid, i);
    return static_cast<s32>(i);
  }

  return FS_EFDEXHAUSTED;
}

IPCReply ESDevice::OpenContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0) || request.in_vectors[0].size != sizeof(u64) ||
      request.in_vectors[1].size != sizeof(ES::TicketView) ||
      request.in_vectors[2].size != sizeof(u32))
  {
    return IPCReply(ES_EINVAL);
  }

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const u32 content_index = Memory::Read_U32(request.in_vectors[2].address);
  // TODO: check the ticket view, check permissions.

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return IPCReply(FS_ENOENT);

  return IPCReply(OpenContent(tmd, content_index, uid));
}

IPCReply ESDevice::OpenActiveTitleContent(u32 caller_uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  const u32 content_index = Memory::Read_U32(request.in_vectors[0].address);

  if (!m_title_context.active)
    return IPCReply(ES_EINVAL);

  ES::UIDSys uid_map{m_ios.GetFS()};
  const u32 uid = uid_map.GetOrInsertUIDForTitle(m_title_context.tmd.GetTitleId());
  if (caller_uid != 0 && caller_uid != uid)
    return IPCReply(ES_EACCES);

  return IPCReply(OpenContent(m_title_context.tmd, content_index, caller_uid));
}

s32 ESDevice::ReadContent(u32 cfd, u8* buffer, u32 size, u32 uid)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;
  OpenedContent& entry = m_content_table[cfd];

  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  const auto result = m_ios.GetFS()->ReadBytesFromFile(entry.m_fd, buffer, size);
  return result.Succeeded() ? *result : FS::ConvertResult(result.Error());
}

IPCReply ESDevice::ReadContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  const u32 cfd = Memory::Read_U32(request.in_vectors[0].address);
  const u32 size = request.io_vectors[0].size;
  const u32 addr = request.io_vectors[0].address;

  return IPCReply(ReadContent(cfd, Memory::GetPointer(addr), size, uid));
}

ReturnCode ESDevice::CloseContent(u32 cfd, u32 uid)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;

  OpenedContent& entry = m_content_table[cfd];
  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  m_ios.GetFS()->Close(entry.m_fd);
  entry = {};
  INFO_LOG_FMT(IOS_ES, "CloseContent: CFD {}", cfd);
  return IPC_SUCCESS;
}

IPCReply ESDevice::CloseContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  const u32 cfd = Memory::Read_U32(request.in_vectors[0].address);
  return IPCReply(CloseContent(cfd, uid));
}

s32 ESDevice::SeekContent(u32 cfd, u32 offset, SeekMode mode, u32 uid)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;

  OpenedContent& entry = m_content_table[cfd];
  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  const auto result = m_ios.GetFS()->SeekFile(entry.m_fd, offset, static_cast<FS::SeekMode>(mode));
  return result.Succeeded() ? *result : FS::ConvertResult(result.Error());
}

IPCReply ESDevice::SeekContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return IPCReply(ES_EINVAL);

  const u32 cfd = Memory::Read_U32(request.in_vectors[0].address);
  const u32 offset = Memory::Read_U32(request.in_vectors[1].address);
  const SeekMode mode = static_cast<SeekMode>(Memory::Read_U32(request.in_vectors[2].address));

  return IPCReply(SeekContent(cfd, offset, mode, uid));
}
}  // namespace IOS::HLE
