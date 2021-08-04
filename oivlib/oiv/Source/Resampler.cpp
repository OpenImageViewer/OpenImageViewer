#include "Resampler.h"
#include <algorithm>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/Exception.h>
#include <intrin.h>
#include <System.h>


namespace OIV
{


	void Resampler::Init()
	{
		//Lazy initialize.
		if (fInitialized == false)
		{
			fNumOfIdealThreadsForResampling = System::GetIdealNumThreadsForMemoryOperations();
			fTasks.resize(fNumOfIdealThreadsForResampling);
			fThreads.resize(fNumOfIdealThreadsForResampling);
			fInitialized = true;
		}

	}

	void Resampler::Resample(const ResamplerParams& params)
	{
		Init();

		const double ratiox = static_cast<double>(params.sourceWidth) / params.targetWidth;
		const double ratioy = static_cast<double>(params.sourceHeight) / params.targetHeight;

		const int32_t diffHor = static_cast<int32_t>(std::round(ratiox) / 2.0);
		const int32_t diffVert = static_cast<int32_t>(std::round(ratioy) / 2.0);


		ResamplerBox box{ -diffHor,-diffVert, diffHor,diffVert };

		if (box.right == 0)
			box.right = 1;
		if (box.bottom == 0)
			box.bottom = 1;


		const int totalThreads = fNumOfIdealThreadsForResampling;
		const size_t totalPixels = params.targetWidth * params.targetHeight;
		//uint32_t resampledRowPitch = sourceImage->GetRowPitchInBytes();
		//uint32_t pixelSize = sourceImage->GetBytesPerTexel();

		//uint32_t * sourceBuffer = const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(sourceImage->GetBufferAt(0, 0)));
		//uint32_t * targetBuffer = const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(resampled->GetBufferAt(0, 0)));

#if RESAMPLE_THREAD_POOL
		static std::atomic_int count = 0;
#endif

		ResampleTask templateTask;
		templateTask.resampleParams = params;
		templateTask.box = box;

		templateTask.ratioX = ratiox;
		templateTask.ratioY = ratioy;
		templateTask.TaskID = 1000;
		templateTask.totalThreads = totalThreads;
		templateTask.totalTargetTexels = totalPixels;
#if RESAMPLE_THREAD_POOL
		static std::condition_variable allTasksDone;
		static std::condition_variable cv;
		static std::mutex mtx;
		static std::mutex mtx1;


		static std::function<void()> callback = []()
		{
			if (++count == totalThreads)
			{
				allTasksDone.notify_one();
			}
		};
		templateTask.Callback = &callback;
		templateTask.cv = &cv;
		templateTask.mtx = &mtx;


		static std::vector<ResampleTask> tasks(totalThreads);
#endif

#if 0
		bool static created = false;

		if (created == false)
		{
			for (int i = 0; i < totalThreads; i++)
			{
				tasks[i] = templateTask;
				threads[i] = std::thread(&ResampleUnit, &tasks[i]);


			}

			while (count < totalThreads)
			{
				Sleep(0);
			}

			//std::unique_lock<std::mutex> lock(mtx1);
			//allTasksDone.wait(lock);


			//while (count < totalThreads)
			//{
			//	Sleep(1);
			//}
			//All threads has been initialzied.


			created = true;
		}




		count = 0;
		for (int i = 0; i < totalThreads; i++)
		{
			tasks[i] = templateTask;
			tasks[i].TaskID = i;
		}

		cv.notify_all();

		std::unique_lock<std::mutex> lock(mtx1);
		allTasksDone.wait(lock);

		//cv.notify_all();
#endif

		for (int i = 0; i < totalThreads; i++)
		{
			fTasks[i] = templateTask;;
			fTasks[i].TaskID = i;
			fThreads[i] = std::thread(std::bind(&Resampler::ResampleThreadEntryPoint, this, std::placeholders::_1), &fTasks[i]);
		}

		for (int i = 0; i < totalThreads; i++)
			fThreads[i].join();

#if 0
		for (int i = 0; i < totalThreads; i++)
		{
			int threadnumber = i;
			threads[i] = std::thread([&, threadnumber]
				{
					uint32_t currentTargetPixel = threadnumber;

					AverageParams params1;
					params1.horSpan = diffHor;
					params1.vertSpan = diffVert;
					params1.imageBuffer = sourceBuffer;
					params1.ImageHeight = sourceImage->GetHeight();
					params1.ImageWidth = sourceImage->GetWidth();

					while (currentTargetPixel < totalPixels)
					{
						const uint32_t targetY = currentTargetPixel / width;
						const uint32_t targetX = currentTargetPixel % width;

						params1.ImageX = static_cast<uint32_t>((targetX + 0.5) * ratiox);
						params1.ImageY = static_cast<uint32_t>((targetY + 0.5) * ratioy);

						targetBuffer[currentTargetPixel] = GetAverageAt(params1);
						currentTargetPixel += totalThreads;
					}
				}
			).detach();
		}
#endif
	}


	__forceinline uint32_t  Resampler::GetAverageAt(const AverageParams& params)
	{
#pragma pack(push,1)
		struct Color
		{
			uint8_t b;
			uint8_t g;
			uint8_t r;
			uint8_t a;
		};


		struct Color16
		{
			uint16_t b;
			uint16_t g;
			uint16_t r;
			uint16_t a;
		};

		struct Color32
		{
			uint32_t b;
			uint32_t g;
			uint32_t r;
			uint32_t a;
		};


		alignas(16) Color32 accum {};



		//alignas(16) thread_local static Color RGBA_PAIR[8192];

		//alignas(16)  Color16 accumColor[2]{};

		const size_t sourceYStart = std::max<size_t>(0, params.box.top + params.ImageY);
		const size_t sourceYEnd = std::min<size_t>(params.ImageHeight, params.ImageY + params.box.bottom);
		const size_t sourcexStart = std::max<size_t>(0, params.box.left + params.ImageX);
		const size_t sourceXEnd = std::min<size_t>(params.ImageWidth, params.ImageX + params.box.right);


		const size_t horSpan = sourceXEnd - sourcexStart;
		//const int verSpan = sourceYEnd - sourceYStart;

		const size_t totalPixels = (sourceXEnd - sourcexStart) * (sourceYEnd - sourceYStart);


#pragma pack(pop,1)
#if 0

		//method 1
		int currentPixel = 0;
		while (currentPixel < totalPixels)
		{

			const uint32_t sourceY = currentPixel / horSpan;
			const uint32_t sourceX = currentPixel % horSpan;

			const IMUtil::PixelUtil::BitTexel32Ex c = *reinterpret_cast<const IMUtil::PixelUtil::BitTexel32Ex*>
				(params.imageBuffer + (sourceX + sourcexStart) + (sourceY + sourceYStart) * params.ImageWidth);
			//accum.A += c.W;
			accum.R += c.Z;
			accum.G += c.Y;
			accum.B += c.X;

			currentPixel++;
		}

#elif 0
		//method 2
		int sourceX = sourcexStart;
		int sourceY = sourceYStart;
		while (sourceX < sourceXEnd)
		{
			const IMUtil::PixelUtil::BitTexel32Ex c = *reinterpret_cast<const IMUtil::PixelUtil::BitTexel32Ex*>
				(params.imageBuffer + sourceX + sourceY * params.ImageWidth);
			//accum.A += c.W;
			accum.R += c.Z;
			accum.G += c.Y;
			accum.B += c.X;

			if (++sourceX == sourceXEnd)
			{
				sourceX = sourcexStart;
				if (++sourceY == sourceYEnd)
					break;
			}
		}

#elif 1
		//method 3

		//_mm256_lo
		//__m256i v1(;
		//__m256i v2(= 12;
		//__m256i v3 = _mm256_add_epi16(v1, v2);


		for (size_t sourceY = sourceYStart; sourceY < sourceYEnd; sourceY++)
			for (size_t sourceX = sourcexStart; sourceX < sourceXEnd; sourceX++)
			{
				//RGBA_PAIR [i++] = *reinterpret_cast<const Color*>(params.imageBuffer + sourceX + sourceY * params.ImageWidth);

				alignas(16) const Color* c = reinterpret_cast<const Color*>(params.imageBuffer + sourceX + sourceY * params.ImageWidth);
				accum.b += c->b;
				accum.g += c->g;
				accum.r += c->r;
				accum.a += c->a;
			}


		alignas(16) Color c {
			static_cast<uint8_t>(accum.b / totalPixels)
				, static_cast<uint8_t>   (accum.g / totalPixels)
				, static_cast<uint8_t>   (accum.r / totalPixels)
				, static_cast<uint8_t>   (accum.a / totalPixels)
		};



		/*Color c{ static_cast<uint8_t>((accumColor[0].b + accumColor[1].b) / totalPixels)
			, static_cast<uint8_t>   ((accumColor[0].g + accumColor[1].g) / totalPixels)
			, static_cast<uint8_t>   ((accumColor[0].r + accumColor[1].r) / totalPixels)
			, static_cast<uint8_t>   ((accumColor[0].a + accumColor[1].a) / totalPixels)
		};*/


#elif 0
		//box 3x3
		//method 3
		const size_t two = 1;
		const uint32_t* bufferStart = params.imageBuffer + sourcexStart + sourceYStart * params.ImageWidth;
		const Color* c0 = reinterpret_cast<const Color*>(bufferStart + 0 + 0 * params.ImageWidth);

		const Color* c1 = reinterpret_cast<const Color*>(bufferStart + 1 + 0 * params.ImageWidth);
		const Color* c2 = reinterpret_cast<const Color*>(bufferStart + two + 0 * params.ImageWidth);
		const Color* c3 = reinterpret_cast<const Color*>(bufferStart + 0 + 1 * params.ImageWidth);
		const Color* c4 = reinterpret_cast<const Color*>(bufferStart + 1 + 1 * params.ImageWidth);
		const Color* c5 = reinterpret_cast<const Color*>(bufferStart + two + 1 * params.ImageWidth);
		const Color* c6 = reinterpret_cast<const Color*>(bufferStart + 0 + two * params.ImageWidth);
		const Color* c7 = reinterpret_cast<const Color*>(bufferStart + 1 + two * params.ImageWidth);
		const Color* c8 = reinterpret_cast<const Color*>(bufferStart + two + two * params.ImageWidth);

		/*accum.A = (c0->a  + c1->a + c2>-a + c3->a + c4->a + c5->a + c6->a + c7->a + c8->a) /;
		accum.R = c0->r;
		accum.G = c0->g;
		accum.B = c0->b;*/

		Color c{ static_cast<uint8_t>((static_cast<size_t>(c0->b) + c1->b + c2->b + c3->b + c4->b + c5->b + c6->b + c7->b + c8->b) / totalPixels)
			, static_cast<uint8_t>((static_cast<size_t>(c0->g) + c1->g + c2->g + c3->g + c4->g + c5->g + c6->g + c7->g + c8->g) / totalPixels)
			, static_cast<uint8_t>((static_cast<size_t>(c0->r) + c1->r + c2->r + c3->r + c4->r + c5->r + c6->r + c7->r + c8->r) / totalPixels)
			, static_cast<uint8_t>((static_cast<size_t>(c0->a) + c1->a + c2->a + c3->a + c4->a + c5->a + c6->a + c7->a + c8->a) / totalPixels)
		};



#endif



		return *reinterpret_cast<uint32_t*>(&c);
	}


	void  Resampler::ResampleThreadEntryPoint(ResampleTask* task)
	{
#if RESAMPLE_THREAD_POOL
		//while (true)
		{
			//if (task->TaskID < 100)
			{
#endif
				const size_t totalTargetTexels = task->totalTargetTexels;
				const size_t targetWidth = task->resampleParams.targetWidth;
				const size_t targetHeight = task->resampleParams.targetHeight;
				const double ratiox = task->ratioX;
				const double ratioy = task->ratioY;
				const uint32_t* sourceBuffer = task->resampleParams.sourceBuffer;
				const size_t totalThreads = task->totalThreads;
				const size_t sourceWidth = task->resampleParams.sourceWidth;
				const size_t sourceHeight = task->resampleParams.sourceHeight;


				uint32_t* targetBuffer = task->resampleParams.targetBuffer;

				AverageParams params1;
				params1.imageBuffer = sourceBuffer;
				params1.ImageHeight = sourceHeight;
				params1.ImageWidth = sourceWidth;
				params1.box = task->box;

				const size_t startY = task->TaskID * (targetHeight / totalThreads);
				const size_t endY = task->TaskID == totalThreads - 1 ? targetHeight : (task->TaskID + 1) * (targetHeight / totalThreads);


				for (size_t targetY = startY; targetY < endY; targetY++)
					for (size_t targetX = 0; targetX < targetWidth; targetX++)
						//while (currentTargetPixel < totalTargetTexels)
					{
						//				const size_t targetY = currentTargetPixel / targetWidth;
										//const size_t targetX = currentTargetPixel - targetY * targetWidth;

						params1.ImageX = static_cast<size_t>((targetX + 0.5) * ratiox + 0.5); // adding 0.5 before casting instead of rounding
						params1.ImageY = static_cast<size_t>((targetY + 0.5) * ratioy + 0.5); // much faster solution.

						targetBuffer[targetY * targetWidth + targetX] = GetAverageAt(params1);
						//targetBuffer[currentTargetPixel] = GetAverageAt(params1);
						//currentTargetPixel += totalThreads;
					}

#if RESAMPLE_THREAD_POOL
			}

			(*task->Callback)();
			std::unique_lock<std::mutex> lock(*task->mtx);
			task->cv->wait(lock);

		}
#endif
	}
}