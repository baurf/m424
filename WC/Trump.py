import time
import RPi.GPIO as GPIO
from pygame import mixer
import pygame
import random


# Pins definitions
btn_pin = 4

# Set up pins
GPIO.setmode(GPIO.BCM)
GPIO.setup(btn_pin, GPIO.IN)
GPIO.setup(17, GPIO.OUT)
GPIO.setup(18, GPIO.OUT)
GPIO.setup(22, GPIO.OUT)
GPIO.setup(23, GPIO.OUT)

# Initialize pygame mixer
pygame.init()
MUSIC_END = pygame.USEREVENT+1
pygame.mixer.music.set_endevent ( pygame.USEREVENT )
pygame.mixer.music.set_endevent(MUSIC_END)

# Remember the current and previous button states
current_state = True
prev_state = True

is_playing = False

# Load the sounds
time.sleep(2)
_songs = ['Lazy.mp3', 'monkey.mp3', 'Baby.mp3', 'BabyShark.mp3', 'BadGuy.mp3', 'BarbieGirl.mp3', 'Batman.mp3', 'Believer.mp3', 'Bohemian.mp3', 'Despacito.mp3', 'GiveUp.mp3', 'Havana.mp3', 'Ibiza.mp3', 'MadeMeDo.mp3', 'Me.mp3', 'OldTown.mp3', 'Psycho.mp3', 'Racist.mp3', 'Senorita.mp3', 'ShakeIt.mp3', 'SingleLadies.mp3', 'Starship.mp3', 'TheBox.mp3', 'WrapPlastic.mp3', 'WreckingBall.mp3', 'Sex.mp3']
next_song = random.choice(_songs)

# If button is pushed, light up LED
try:
    while True:
        current_state = GPIO.input(btn_pin)
        if (current_state == False) and (prev_state == True):
            print("playing")
            pygame.mixer.music.load(next_song)
            pygame.mixer.music.play()
            _currently_playing_song = next_song

            is_playing = True

            time.sleep(1)
            
            while next_song == _currently_playing_song:
                next_song = random.choice(_songs)

            pygame.mixer.music.queue(next_song)
            
        if (current_state == True) and (prev_state == False):
            time.sleep(2)
            pygame.mixer.music.stop()
            print("Stopped")
            is_playing = False
            
        prev_state = current_state

        if (current_state == False) and (pygame.mixer.music.get_busy() == False):
            prev_state = True

        if (is_playing):
            GPIO.output(17, True)
            time.sleep(0.3)
            GPIO.output(17, False)
            time.sleep(0.1)
            GPIO.output(18, True)
            time.sleep(0.3)
            GPIO.output(18, False)
            time.sleep(0.1)
            GPIO.output(22, True)
            time.sleep(0.3)
            GPIO.output(22, False)
            time.sleep(0.1)
            GPIO.output(23, True)
            time.sleep(0.3)
            GPIO.output(23, False)
        else:
            time.sleep(0.1)
            
        

# When you press ctrl+c, this will be called
finally:
    GPIO.cleanup()