/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author:Mark Yao <mark.yao@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>
#include <drm/rockchip_drm.h>
#include <linux/iommu.h>

#include <linux/dma-attrs.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_gem.h"

static int rockchip_gem_iommu_map(struct rockchip_drm_private *private,
				  struct rockchip_gem_object *rk_obj)
{
	int prot = IOMMU_READ | IOMMU_WRITE;
	ssize_t ret;

	if (rk_obj->mm)
		return -EBUSY;

	rk_obj->mm = kzalloc(sizeof(*rk_obj->mm), GFP_KERNEL);
	if (!rk_obj->mm)
		return -ENOMEM;

	ret = drm_mm_insert_node_generic(&private->mm, rk_obj->mm,
					 rk_obj->base.size, PAGE_SIZE,
					 0, 0, 0);
	if (ret < 0) {
		DRM_ERROR("out of I/O virtual memory: %zd\n", ret);
		goto err_free_mm;
	}

	rk_obj->dma_addr = rk_obj->mm->start;

	ret = iommu_map_sg(private->domain, rk_obj->dma_addr, rk_obj->sgt->sgl,
			   rk_obj->sgt->nents, prot);
	if (ret < 0) {
		DRM_ERROR("failed to map buffer: %zd\n", ret);
		goto err_remove_node;
	}

	rk_obj->size = ret;

	return 0;

err_remove_node:
	drm_mm_remove_node(rk_obj->mm);
err_free_mm:
	kfree(rk_obj->mm);
	return ret;
}

static int rockchip_gem_iommu_unmap(struct rockchip_drm_private *private,
				    struct rockchip_gem_object *rk_obj)
{
	if (!rk_obj->mm)
		return 0;

	iommu_unmap(private->domain, rk_obj->dma_addr, rk_obj->size);
	drm_mm_remove_node(rk_obj->mm);
	kfree(rk_obj->mm);

	return 0;
}

static int rockchip_gem_get_pages(struct drm_device *drm,
				  struct rockchip_gem_object *rk_obj)
{
	int ret, i;
	struct scatterlist *s;

	rk_obj->pages = drm_gem_get_pages(&rk_obj->base);
	if (IS_ERR(rk_obj->pages))
		return PTR_ERR(rk_obj->pages);

	rk_obj->num_pages = rk_obj->base.size >> PAGE_SHIFT;

	rk_obj->sgt = drm_prime_pages_to_sg(rk_obj->pages, rk_obj->num_pages);
	if (IS_ERR(rk_obj->sgt)) {
		ret = PTR_ERR(rk_obj->sgt);
		goto err_put_pages;
	}

	/*
	 * Fake up the SG table so that dma_sync_sg_for_device() can be used
	 * to flush the pages associated with it.
	 *
	 * TODO: Replace this by drm_clflash_sg() once it can be implemented
	 * without relying on symbols that are not exported.
	 */
	for_each_sg(rk_obj->sgt->sgl, s, rk_obj->sgt->nents, i)
		sg_dma_address(s) = sg_phys(s);

	dma_sync_sg_for_device(drm->dev, rk_obj->sgt->sgl, rk_obj->sgt->nents,
			       DMA_TO_DEVICE);

	return 0;

err_put_pages:
	drm_gem_put_pages(&rk_obj->base, rk_obj->pages, false, false);
	return ret;
}

static void rockchip_gem_put_pages(struct drm_device *drm,
				   struct rockchip_gem_object *rk_obj)
{
	sg_free_table(rk_obj->sgt);
	kfree(rk_obj->sgt);
	drm_gem_put_pages(&rk_obj->base, rk_obj->pages, false, false);
}

static int rockchip_gem_alloc_iommu(struct drm_device *drm,
				    struct rockchip_gem_object *rk_obj,
				    bool alloc_kmap)
{
	struct rockchip_drm_private *private = drm->dev_private;
	int ret;

	ret = rockchip_gem_get_pages(drm, rk_obj);
	if (ret < 0)
		return ret;

	ret = rockchip_gem_iommu_map(private, rk_obj);
	if (ret < 0)
		goto err_free;

	if (alloc_kmap) {
		rk_obj->kvaddr = vmap(rk_obj->pages, rk_obj->num_pages, VM_MAP,
				      pgprot_writecombine(PAGE_KERNEL));
		if (!rk_obj->kvaddr) {
			DRM_ERROR("failed to vmap() framebuffer\n");
			ret = -ENOMEM;
			goto err_unmap;
		}
	}

	return 0;

err_unmap:
	rockchip_gem_iommu_unmap(private, rk_obj);
err_free:
	rockchip_gem_put_pages(drm, rk_obj);

	return ret;
}

static int rockchip_gem_alloc_dma(struct rockchip_gem_object *rk_obj,
				  bool alloc_kmap)
{
	struct drm_gem_object *obj = &rk_obj->base;
	struct drm_device *drm = obj->dev;

	init_dma_attrs(&rk_obj->dma_attrs);
	dma_set_attr(DMA_ATTR_WRITE_COMBINE, &rk_obj->dma_attrs);

	if (!alloc_kmap)
		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &rk_obj->dma_attrs);

	rk_obj->kvaddr = dma_alloc_attrs(drm->dev, obj->size,
					 &rk_obj->dma_addr, GFP_KERNEL,
					 &rk_obj->dma_attrs);
	if (!rk_obj->kvaddr) {
		DRM_ERROR("failed to allocate %zu byte dma buffer", obj->size);
		return -ENOMEM;
	}

	return 0;
}

static int rockchip_gem_alloc_buf(struct rockchip_gem_object *rk_obj,
				  bool alloc_kmap)
{
	struct drm_gem_object *obj = &rk_obj->base;
	struct drm_device *drm = obj->dev;
	struct rockchip_drm_private *private = drm->dev_private;

	if (private->domain)
		return rockchip_gem_alloc_iommu(drm, rk_obj, alloc_kmap);
	else
		return rockchip_gem_alloc_dma(rk_obj, alloc_kmap);
}

static void rockchip_gem_free_iommu(struct rockchip_gem_object *rk_obj)
{
	struct drm_gem_object *obj = &rk_obj->base;
	struct drm_device *drm = obj->dev;
	struct rockchip_drm_private *private = drm->dev_private;

	vunmap(rk_obj->kvaddr);
	rockchip_gem_iommu_unmap(private, rk_obj);
	rockchip_gem_put_pages(drm, rk_obj);
}

static void rockchip_gem_free_dma(struct rockchip_gem_object *rk_obj)
{
	struct drm_gem_object *obj = &rk_obj->base;
	struct drm_device *drm = obj->dev;

	dma_free_attrs(drm->dev, obj->size, rk_obj->kvaddr,
		       rk_obj->dma_addr, &rk_obj->dma_attrs);
}

static void rockchip_gem_free_buf(struct rockchip_gem_object *rk_obj)
{
	if (rk_obj->pages)
		rockchip_gem_free_iommu(rk_obj);
	else
		rockchip_gem_free_dma(rk_obj);
}

static int rockchip_drm_gem_object_mmap_iommu(struct drm_gem_object *obj,
					      struct vm_area_struct *vma)
{
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);
	unsigned int i, count = PAGE_ALIGN(obj->size) >> PAGE_SHIFT;
	unsigned long user_count = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	unsigned long uaddr = vma->vm_start;
	int ret = -ENXIO;

	if (user_count > count)
		return -ENXIO;

	for (i = 0; i < user_count; i++) {
		ret = vm_insert_page(vma, uaddr, rk_obj->pages[i]);
		if (ret)
			break;
		uaddr += PAGE_SIZE;
	}

	return ret;
}

static int rockchip_drm_gem_object_mmap_dma(struct drm_gem_object *obj,
					    struct vm_area_struct *vma)
{
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);
	struct drm_device *drm = obj->dev;

	return dma_mmap_attrs(drm->dev, vma, rk_obj->kvaddr, rk_obj->dma_addr,
			      obj->size, &rk_obj->dma_attrs);
}

static int rockchip_drm_gem_object_mmap(struct drm_gem_object *obj,
					struct vm_area_struct *vma)
{
	int ret;
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);

	/*
	 * We allocated a struct page table for rk_obj, so clear
	 * VM_PFNMAP flag that was set by drm_gem_mmap_obj()/drm_gem_mmap().
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	if (rk_obj->pages)
		ret = rockchip_drm_gem_object_mmap_iommu(obj, vma);
	else
		ret = rockchip_drm_gem_object_mmap_dma(obj, vma);

	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}

int rockchip_gem_mmap_buf(struct drm_gem_object *obj,
			  struct vm_area_struct *vma)
{
	int ret;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret)
		return ret;

	return rockchip_drm_gem_object_mmap(obj, vma);
}

/* drm driver mmap file operations */
int rockchip_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;

	return rockchip_drm_gem_object_mmap(obj, vma);
}

struct rockchip_gem_object *
	rockchip_gem_create_object(struct drm_device *drm, unsigned int size,
				   bool alloc_kmap)
{
	struct rockchip_gem_object *rk_obj;
	struct drm_gem_object *obj;
	int ret;

	size = round_up(size, PAGE_SIZE);

	rk_obj = kzalloc(sizeof(*rk_obj), GFP_KERNEL);
	if (!rk_obj)
		return ERR_PTR(-ENOMEM);

	obj = &rk_obj->base;

	drm_gem_object_init(drm, obj, size);

	ret = rockchip_gem_alloc_buf(rk_obj, alloc_kmap);
	if (ret)
		goto err_free_rk_obj;

	return rk_obj;

err_free_rk_obj:
	kfree(rk_obj);
	return ERR_PTR(ret);
}

/*
 * rockchip_gem_free_object - (struct drm_driver)->gem_free_object callback
 * function
 */
void rockchip_gem_free_object(struct drm_gem_object *obj)
{
	struct rockchip_gem_object *rk_obj;

	drm_gem_free_mmap_offset(obj);

	rk_obj = to_rockchip_obj(obj);

	rockchip_gem_free_buf(rk_obj);

	kfree(rk_obj);
}

/*
 * rockchip_gem_create_with_handle - allocate an object with the given
 * size and create a gem handle on it
 *
 * returns a struct rockchip_gem_object* on success or ERR_PTR values
 * on failure.
 */
static struct rockchip_gem_object *
rockchip_gem_create_with_handle(struct drm_file *file_priv,
				struct drm_device *drm, unsigned int size,
				unsigned int *handle)
{
	struct rockchip_gem_object *rk_obj;
	struct drm_gem_object *obj;
	int ret;

	rk_obj = rockchip_gem_create_object(drm, size, false);
	if (IS_ERR(rk_obj))
		return ERR_CAST(rk_obj);

	obj = &rk_obj->base;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, obj, handle);
	if (ret)
		goto err_handle_create;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(obj);

	return rk_obj;

err_handle_create:
	rockchip_gem_free_object(obj);

	return ERR_PTR(ret);
}

int rockchip_gem_dumb_map_offset(struct drm_file *file_priv,
				 struct drm_device *dev, uint32_t handle,
				 uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return -EINVAL;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG_KMS("offset = 0x%llx\n", *offset);

out:
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

/*
 * rockchip_gem_dumb_create - (struct drm_driver)->dumb_create callback
 * function
 *
 * This aligns the pitch and size arguments to the minimum required. wrap
 * this into your own function if you need bigger alignment.
 */
int rockchip_gem_dumb_create(struct drm_file *file_priv,
			     struct drm_device *dev,
			     struct drm_mode_create_dumb *args)
{
	struct rockchip_gem_object *rk_obj;
	int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	/*
	 * align to 64 bytes since Mali requires it.
	 */
	args->pitch = ALIGN(min_pitch, 64);
	args->size = args->pitch * args->height;

	rk_obj = rockchip_gem_create_with_handle(file_priv, dev, args->size,
						 &args->handle);

	return PTR_ERR_OR_ZERO(rk_obj);
}

int rockchip_gem_map_offset_ioctl(struct drm_device *drm, void *data,
				  struct drm_file *file_priv)
{
	struct drm_rockchip_gem_map_off *args = data;

	return rockchip_gem_dumb_map_offset(file_priv, drm, args->handle,
					    &args->offset);
}

int rockchip_gem_create_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_rockchip_gem_create *args = data;
	struct rockchip_gem_object *rk_obj;

	rk_obj = rockchip_gem_create_with_handle(file_priv, dev, args->size,
						 &args->handle);
	return PTR_ERR_OR_ZERO(rk_obj);
}

/*
 * Allocate a sg_table for this GEM object.
 * Note: Both the table's contents, and the sg_table itself must be freed by
 *       the caller.
 * Returns a pointer to the newly allocated sg_table, or an ERR_PTR() error.
 */
struct sg_table *rockchip_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);
	struct drm_device *drm = obj->dev;
	struct sg_table *sgt;
	int ret;

	if (rk_obj->pages)
		return drm_prime_pages_to_sg(rk_obj->pages, rk_obj->num_pages);

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = dma_get_sgtable_attrs(drm->dev, sgt, rk_obj->kvaddr,
				    rk_obj->dma_addr, obj->size,
				    &rk_obj->dma_attrs);
	if (ret) {
		DRM_ERROR("failed to allocate sgt, %d\n", ret);
		kfree(sgt);
		return ERR_PTR(ret);
	}

	return sgt;
}

void *rockchip_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);

	if (rk_obj->pages)
		return vmap(rk_obj->pages, rk_obj->num_pages, VM_MAP,
			    pgprot_writecombine(PAGE_KERNEL));

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, &rk_obj->dma_attrs))
		return NULL;

	return rk_obj->kvaddr;
}

void rockchip_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);

	/* Nothing to do if allocated by DMA mapping API. */
	if (!rk_obj->pages)
		return;

	vunmap(vaddr);
}
